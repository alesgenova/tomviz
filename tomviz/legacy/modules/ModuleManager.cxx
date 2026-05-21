/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleManager.h"

#include "ActiveObjects.h"
#include "ViewsLayoutsSerializer.h"
#include "../DataSource.h"
#include "../operators/Operator.h"
#include "LoadDataReaction.h"
#include "ModuleFactory.h"
#include "MoleculeSource.h"
#include "../Pipeline.h"
#include "../PythonGeneratedDatasetReaction.h"

#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"
#include "Utilities.h"
#include "tomvizConfig.h"

#include <pqActiveObjects.h>
#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqApplicationCore.h>
#include <pqDeleteReaction.h>
#include <pqPVApplicationCore.h>
#include <pqServerManagerModel.h>
#include <pqSettings.h>

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkPVRenderView.h>
#include <vtkPVXMLElement.h>
#include <vtkPVXMLParser.h>
#include <vtkRenderer.h>
#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMProxyLocator.h>
#include <vtkSMProxyManager.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMRepresentationProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTimeKeeper.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>
#include <vtkStdString.h>

#include <vtk_pugixml.h>

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QMap>
#include <QMessageBox>
#include <QMultiMap>
#include <QPointer>
#include <QSet>

#include <sstream>

namespace tomviz {

static QList<QJsonObject> toObjectList(const QJsonArray& array)
{
  QList<QJsonObject> ret;
  for (const auto& item : array) {
    ret.append(item.toObject());
  }
  return ret;
}

class ModuleManager::MMInternals
{
public:
  // TODO Should only hold top level roots of pipeline
  QList<QPointer<DataSource>> DataSources;
  QList<QPointer<MoleculeSource>> MoleculeSources;
  QList<QPointer<DataSource>> ChildDataSources;
  QList<QPointer<Module>> Modules;
  QMap<vtkSMProxy*, vtkSmartPointer<vtkCamera>> RenderViewCameras;

  // Map from view proxies to modules. Used to keep track of how many modules
  // have been added to a view.
  QMultiMap<vtkSMProxy*, Module*> ViewModules;

  // State for the "state finished loading signal"
  int RemaningPipelinesToWaitFor;
  bool LastStateLoadSuccess;

  // Ensure all pipelines created when restoring the state are not executed
  bool ExecutePipelinesOnLoad = true;

  // Only used by onPVStateLoaded for the second half of deserialize
  QDir dir;
  QMap<vtkTypeUInt32, vtkSMViewProxy*> ViewIdMap;
  void relativeFilePaths(DataSource* ds, const QDir& stateDir,
                         QJsonObject& dataSourceState)
  {
    QJsonObject readerProps;
    if (dataSourceState.contains("reader") &&
        dataSourceState["reader"].isObject()) {
      readerProps = dataSourceState["reader"].toObject();
    }

    // Make any reader fileName properties relative to the state file being
    // written.
    if (readerProps.contains("fileNames")) {
      // Exclude transient data sources.
      // ( ones without a file. i.e. output data sources )
      if (!ds->isTransient()) {
        auto fileNames = readerProps["fileNames"].toArray();
        QJsonArray relativeNames;
        foreach (QJsonValue name, fileNames) {
          relativeNames.append(stateDir.relativeFilePath(name.toString()));
        }
        readerProps["fileNames"] = relativeNames;
      }
      dataSourceState["reader"] = readerProps;
    }
  }

  void relativeFilePaths(MoleculeSource*, const QDir& stateDir,
                         QJsonObject& dataSourceState)
  {
    QJsonObject readerProps;
    if (dataSourceState.contains("reader") &&
        dataSourceState["reader"].isObject()) {
      readerProps = dataSourceState["reader"].toObject();
    }

    // Make any reader fileName properties relative to the state file being
    // written.
    if (readerProps.contains("fileName")) {
      auto fileName = readerProps["fileName"].toString();
      readerProps["fileName"] = stateDir.relativeFilePath(fileName);
      dataSourceState["reader"] = readerProps;
    }
  }

  void absoluteFilePaths(QJsonObject& dataSourceState)
  {

    std::function<QString(QString)> absolute = [this](QString path) {
      if (!path.isEmpty()) {
        path = QDir::cleanPath(this->dir.absoluteFilePath(path));
      }

      return path;
    };

    if (dataSourceState.contains("reader") &&
        dataSourceState["reader"].isObject()) {
      auto reader = dataSourceState["reader"].toObject();
      if (reader.contains("fileNames") && reader["fileNames"].isArray()) {
        auto fileNames = reader["fileNames"].toArray();
        QJsonArray absoluteFileNames;
        foreach (const QJsonValue& path, fileNames) {
          absoluteFileNames.append(absolute(path.toString()));
        }
        reader["fileNames"] = absoluteFileNames;
      }
      if (reader.contains("fileName") && reader["fileName"].isString()) {
        QString absoluteFileName = absolute(reader["fileName"].toString());
        reader["fileName"] = absoluteFileName;
      }
      dataSourceState["reader"] = reader;
    }
  }

  QStringList dataSourceDependencies(const QJsonObject& ds)
  {
    // Traverse recursively through the operators and find any
    // data source dependencies. Returns a list of their ids.
    QStringList deps;
    for (auto opVal : ds.value("operators").toArray()) {
      auto op = opVal.toObject();
      auto args = op.value("arguments").toObject();
      for (auto& key : args.keys()) {
        auto value = args[key];
        if (value.isString() && value.toString().startsWith("0x")) {
          // This is a dependency, add it.
          deps.append(value.toString());
        }
      }

      for (auto childVal : op.value("dataSources").toArray()) {
        // Recursively add child dependencies.
        deps.append(dataSourceDependencies(childVal.toObject()));
      }
    }

    return deps;
  }

  QJsonObject rootDataSourceDependency(QString dep,
                                       QList<QJsonObject>& dataSources)
  {
    // Find the root data source of this dependency.
    // Searches through the list of dataSources to find this information.
    for (auto& ds : dataSources) {
      if (ds.value("id").toString() == dep) {
        return ds;
      }

      for (auto opVal : ds.value("operators").toArray()) {
        auto op = opVal.toObject();
        auto children = toObjectList(op.value("dataSources").toArray());

        // Recursively search for it in the children
        auto result = rootDataSourceDependency(dep, children);
        if (!result.isEmpty()) {
          return ds;
        }
      }
    }

    // Didn't find it...
    return {};
  }

  bool isChildDataSourceDependency(QString dep, QList<QJsonObject>& dataSources)
  {
    auto root = rootDataSourceDependency(dep, dataSources);
    return root.value("id").toString() != dep;
  }

  void removeInvalidDataSources()
  {
    int i = 0;
    while (i < DataSources.size()) {
      if (DataSources[i]) {
        // Valid
        i += 1;
        continue;
      } else {
        // Invalid. Remove.
        DataSources.removeAt(i);
      }
    }
  }

  void removeInvalidChildDataSources()
  {
    int i = 0;
    while (i < ChildDataSources.size()) {
      if (ChildDataSources[i]) {
        // Valid
        i += 1;
        continue;
      } else {
        // Invalid. Remove.
        ChildDataSources.removeAt(i);
      }
    }
  }
};

ModuleManager::ModuleManager(QObject* parentObject)
  : Superclass(parentObject), d(new ModuleManager::MMInternals())
{
  connect(pqApplicationCore::instance()->getServerManagerModel(),
          &pqServerManagerModel::viewRemoved, this,
          &ModuleManager::onViewRemoved);
}

ModuleManager::~ModuleManager() = default;

ModuleManager& ModuleManager::instance()
{
  static ModuleManager theInstance;
  return theInstance;
}

void ModuleManager::reset()
{
  removeAllModules();
  removeAllDataSources();
  removeAllMoleculeSources();
  pqDeleteReaction::deleteAll();
}

bool ModuleManager::hasRunningOperators()
{
  d->removeInvalidDataSources();
  for (QPointer<DataSource> dsource : d->DataSources) {
    if (dsource->pipeline()->isRunning()) {
      return true;
    }
  }
  return false;
}

QList<DataSource*> ModuleManager::dataSources()
{
  // Somehow we have invalid data sources around. Make sure
  // those are removed.
  d->removeInvalidDataSources();
  QList<DataSource*> ret;
  std::copy(d->DataSources.begin(), d->DataSources.end(),
            std::back_inserter(ret));
  return ret;
}

QList<DataSource*> ModuleManager::childDataSources()
{
  // Somehow we have invalid data sources around. Make sure
  // those are removed.
  d->removeInvalidChildDataSources();
  QList<DataSource*> ret;
  std::copy(d->ChildDataSources.begin(), d->ChildDataSources.end(),
            std::back_inserter(ret));
  return ret;
}

QList<DataSource*> ModuleManager::allDataSources()
{
  return dataSources() + childDataSources();
}

QList<DataSource*> ModuleManager::allDataSourcesDepthFirst()
{
  // Return the data sources in a Depth First Search (DFS) order,
  // so that child data sources will come immediately after their
  // parent data source in the list.
  // This *should* match the order of data sources in the pipeline view.
  QList<DataSource*> result;
  for (auto* rootSource : dataSources()) {
    result.append(rootSource);
    if (rootSource->operators().isEmpty()) {
      // If there are no operators, there are no child data sources...
      continue;
    }

    auto* lastOperator = rootSource->operators().back();
    auto* childSource = lastOperator->childDataSource();
    if (childSource) {
      result.append(childSource);
    }
  }

  return result;
}

QStringList ModuleManager::createUniqueLabels(const QList<DataSource*>& sources)
{
  QStringList ret;
  for (auto* ds : sources) {
    auto label = ds->label();
    if (ret.contains(label)) {
      auto original = label;
      int index = 1;
      do {
        label = original + QString(" (%1)").arg(++index);
      } while (ret.contains(label));
    }
    ret.append(label);
  }
  return ret;
}

void ModuleManager::addDataSource(DataSource* dataSource)
{
  d->removeInvalidDataSources();
  if (dataSource && !d->DataSources.contains(dataSource)) {
    d->DataSources.push_back(dataSource);
    emit dataSourceAdded(dataSource);
  }
}

void ModuleManager::addChildDataSource(DataSource* dataSource)
{
  d->removeInvalidChildDataSources();
  if (dataSource && !d->ChildDataSources.contains(dataSource)) {
    d->ChildDataSources.push_back(dataSource);
    emit childDataSourceAdded(dataSource);
  }
}

void ModuleManager::removeDataSource(DataSource* dataSource)
{
  d->removeInvalidDataSources();
  d->removeInvalidChildDataSources();
  if (d->DataSources.removeOne(dataSource) ||
      d->ChildDataSources.removeOne(dataSource)) {
    emit dataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
}

void ModuleManager::removeChildDataSource(DataSource* dataSource)
{
  d->removeInvalidChildDataSources();
  if (d->ChildDataSources.removeOne(dataSource)) {
    emit childDataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
}

void ModuleManager::removeAllDataSources()
{
  d->removeInvalidDataSources();
  foreach (DataSource* dataSource, d->DataSources) {
    emit dataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
  d->DataSources.clear();
}

void ModuleManager::removeAllMoleculeSources()
{
  foreach (MoleculeSource* moleculeSource, d->MoleculeSources) {
    emit moleculeSourceRemoved(moleculeSource);
    moleculeSource->deleteLater();
  }
  d->MoleculeSources.clear();
}

void ModuleManager::addMoleculeSource(MoleculeSource* moleculeSource)
{
  if (moleculeSource && !d->MoleculeSources.contains(moleculeSource)) {
    d->MoleculeSources.push_back(moleculeSource);
    emit moleculeSourceAdded(moleculeSource);
  }
}

void ModuleManager::removeMoleculeSource(MoleculeSource* moleculeSource)
{
  if (d->MoleculeSources.removeOne(moleculeSource)) {
    emit moleculeSourceRemoved(moleculeSource);
  }
}

void ModuleManager::removeOperator(Operator* op)
{
  if (op) {
    emit operatorRemoved(op);
  }
}

bool ModuleManager::isChild(DataSource* source) const
{
  d->removeInvalidChildDataSources();
  return (d->ChildDataSources.indexOf(source) >= 0);
}

void ModuleManager::addModule(Module* module)
{
  if (!d->Modules.contains(module)) {
    module->setParent(this);
    d->Modules.push_back(module);

    // Reset display if this is the first module in the view.
    if (d->ViewModules.count(module->view()) == 0) {
      auto pqview = tomviz::convert<pqView*>(module->view());
      pqview->resetDisplay();
      pqview->render();
    }
    d->ViewModules.insert(module->view(), module);

    // Prevent the user from adding more than 6 clipping planes
    // to a data source
    int count = 0;
    foreach (Module* m, d->Modules) {
      if (m->dataSource() == module->dataSource()) {
        if (strcmp(ModuleFactory::moduleType(m), "Clip") == 0) {
          ++count;
        }
      }
    }
    if (count > 6) {
      QMessageBox::warning(tomviz::mainWidget(),
                           tr("Max Clipping Planes Reached"),
                           tr("No more than 6 clipping planes can be added to "
                              "a single data source."),
                           QMessageBox::Ok);
      removeModule(module);
      return;
    }

    emit moduleAdded(module);
    connect(module, &Module::renderNeeded, this, &ModuleManager::render);
    connect(module, &Module::updateClientSideViewNeeded, this,
            &ModuleManager::updateClientSideView);
    if (strcmp(ModuleFactory::moduleType(module), "Slice") == 0) {
      connect(module, &Module::mouseOverVoxel, this,
              &ModuleManager::mouseOverVoxel);
    }
    connect(module, &Module::visibilityChanged, this, &ModuleManager::visibilityChanged);
  }
}

void ModuleManager::removeModule(Module* module)
{
  if (d->Modules.removeOne(module)) {
    d->ViewModules.remove(module->view(), module);
    emit moduleRemoved(module);
    module->deleteLater();
  }
}

void ModuleManager::removeAllModules()
{
  foreach (Module* module, d->Modules) {
    emit moduleRemoved(module);
    module->deleteLater();
  }
  d->Modules.clear();
}

void ModuleManager::removeAllModules(DataSource* source)
{
  Q_ASSERT(source);
  QList<Module*> modules;
  foreach (Module* module, d->Modules) {
    if (module->dataSource() == source) {
      modules.push_back(module);
    }
  }
  foreach (Module* module, modules) {
    removeModule(module);
  }
}

Module* ModuleManager::createAndAddModule(const QString& type,
                                          DataSource* dataSource,
                                          vtkSMViewProxy* view)
{
  if (!view || !dataSource) {
    return nullptr;
  }

  // Create an outline module for the source in the active view.
  auto module = ModuleFactory::createModule(type, dataSource, view);
  if (module) {
    addModule(module);
  }
  return module;
}

Module* ModuleManager::createAndAddModule(const QString& type,
                                          MoleculeSource* moleculeSource,
                                          vtkSMViewProxy* view)
{
  if (!view || !moleculeSource) {
    return nullptr;
  }

  // Create an outline module for the source in the active view.
  auto module = ModuleFactory::createModule(type, moleculeSource, view);
  if (module) {
    addModule(module);
  }
  return module;
}

Module* ModuleManager::createAndAddModule(const QString& type,
                                          OperatorResult* result,
                                          vtkSMViewProxy* view)
{
  if (!view || !result) {
    return nullptr;
  }

  // Create an outline module for the source in the active view.
  auto module = ModuleFactory::createModule(type, result, view);
  if (module) {
    addModule(module);
  }
  return module;
}

QList<Module*> ModuleManager::findModulesGeneric(const DataSource* dataSource,
                                                 const vtkSMViewProxy* view)
{
  QList<Module*> modules;
  foreach (Module* module, d->Modules) {
    if (module && module->dataSource() == dataSource &&
        (view == nullptr || view == module->view()) &&
        module->label() != "Molecule") {
      modules.push_back(module);
    }
  }
  return modules;
}

QList<Module*> ModuleManager::findModulesGeneric(
  const MoleculeSource* dataSource, const vtkSMViewProxy* view)
{
  QList<Module*> modules;
  foreach (Module* module, d->Modules) {
    if (module && module->moleculeSource() == dataSource &&
        (view == nullptr || view == module->view())) {
      modules.push_back(module);
    }
  }
  return modules;
}

QJsonArray jsonArrayFromXml(pugi::xml_node node)
{
  // Simple function, just iterates through the elements and fills the array.
  // This assumes the ParaView Element nodes, values stored in value attributes.
  QJsonArray array;
  for (auto element = node.child("Element"); element;
       element = element.next_sibling("Element")) {
    array.append(element.attribute("value").as_int(-1));
  }
  return array;
}

QJsonArray jsonArrayFromXmlDouble(pugi::xml_node node)
{
  // Simple function, just iterates through the elements and fills the array.
  // This assumes the ParaView Element nodes, values stored in value attributes.
  QJsonArray array;
  for (auto element = node.child("Element"); element;
       element = element.next_sibling("Element")) {
    array.append(element.attribute("value").as_double(-1));
  }
  return array;
}

bool ModuleManager::serialize(QJsonObject& doc, const QDir& stateDir,
                              bool interactive) const
{
  d->removeInvalidDataSources();
  d->removeInvalidChildDataSources();

  QJsonObject tvObj;
  tvObj["version"] = QString(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0) {
    tvObj["versionExtra"] = QString(TOMVIZ_VERSION_EXTRA);
  }

  auto pvVer = QString("%1.%2.%3")
                 .arg(vtkSMProxyManager::GetVersionMajor())
                 .arg(vtkSMProxyManager::GetVersionMinor())
                 .arg(vtkSMProxyManager::GetVersionPatch());
  tvObj["paraViewVersion"] = pvVer;

  doc["tomviz"] = tvObj;

  if (interactive) {
    // Iterate over all data sources and check is there are any that are not
    // currently saved.
    int modified = 0;
    foreach (const QPointer<DataSource>& ds, d->DataSources) {
      if (ds != nullptr &&
          ds->persistenceState() == DataSource::PersistenceState::Modified) {
        ++modified;
      }
    }
    foreach (const QPointer<DataSource>& ds, d->ChildDataSources) {
      if (ds != nullptr &&
          ds->persistenceState() == DataSource::PersistenceState::Modified) {
        ++modified;
      }
    }

    if (modified > 0) {
      QMessageBox modifiedMessageBox;
      modifiedMessageBox.setIcon(QMessageBox::Warning);
      QString text = QString("Warning: unsaved data - %1 data source%2")
                       .arg(modified)
                       .arg(modified > 1 ? "s" : "");
      QString infoText =
        "Unsaved data is marked in the pipeline italic text "
        "with an asterisk. You may continue to save the state, "
        "and any unsaved data (along with operators/modules) "
        "will be skipped.";
      modifiedMessageBox.setText(text);
      modifiedMessageBox.setInformativeText(infoText);
      modifiedMessageBox.setStandardButtons(QMessageBox::Save |
                                            QMessageBox::Cancel);
      modifiedMessageBox.setDefaultButton(QMessageBox::Save);

      if (modifiedMessageBox.exec() == QMessageBox::Cancel) {
        return false;
      }
    }
  }

  QJsonArray jDataSources;
  foreach (DataSource* ds, d->DataSources) {
    auto jDataSource = ds->serialize();
    d->relativeFilePaths(ds, stateDir, jDataSource);
    jDataSources.append(jDataSource);
  }
  doc["dataSources"] = jDataSources;

  QJsonArray jMoleculeSources;
  foreach (MoleculeSource* ms, d->MoleculeSources) {
    auto jMoleculeSource = ms->serialize();
    // LEGACY STUB: activeMoleculeSource() removed from ActiveObjects
    if (false) {
      jMoleculeSource["active"] = true;
    }

    d->relativeFilePaths(ms, stateDir, jMoleculeSource);

    jMoleculeSources.append(jMoleculeSource);
  }
  doc["moleculeSources"] = jMoleculeSources;

  // Palette color, views, and layouts share the same emitter between the
  // legacy and new state-file formats.
  ViewsLayoutsSerializer::saveActive(doc);

  return true;
}

void createXmlProperty(pugi::xml_node& n, const char* name, int id)
{
  n.set_name("Property");
  n.append_attribute("name").set_value(name);
  QString idStr = QString::number(id) + "." + name;
  n.append_attribute("id").set_value(idStr.toStdString().c_str());
}

template <typename T>
void createXmlProperty(pugi::xml_node& n, const char* name, int id, T value)
{
  createXmlProperty(n, name, id);
  n.append_attribute("number_of_elements").set_value(1);
  auto element = n.append_child("Element");
  element.append_attribute("index").set_value(0);
  element.append_attribute("value").set_value(value);
}

void createXmlProperty(pugi::xml_node& n, const char* name, int id,
                       QJsonArray arr)
{
  createXmlProperty(n, name, id);
  n.append_attribute("number_of_elements").set_value(arr.size());
  for (int i = 0; i < arr.size(); ++i) {
    auto element = n.append_child("Element");
    element.append_attribute("index").set_value(i);
    element.append_attribute("value").set_value(arr[i].toDouble(-1));
  }
}

void createXmlLayout(pugi::xml_node& n, QJsonArray arr)
{
  n.set_name("Layout");
  n.append_attribute("number_of_elements").set_value(arr.size());
  for (int i = 0; i < arr.size(); ++i) {
    auto obj = arr[i].toObject();
    auto item = n.append_child("Item");
    item.append_attribute("direction").set_value(obj["direction"].toInt());
    item.append_attribute("fraction").set_value(obj["fraction"].toDouble());
    item.append_attribute("view").set_value(obj["viewId"].toInt());
  }
}

bool ModuleManager::deserialize(const QJsonObject& doc, const QDir& stateDir,
                                bool loadDataSources)
{
  m_isDeserializing = true;

  // Get back to a known state.
  reset();
  d->LastStateLoadSuccess = true;

  // Disable the contour module's dialog, re-enable it when the state loading is
  // finished.
  QSettings* settings = pqApplicationCore::instance()->settings();
  bool userConfirmInitialValue =
    settings->value("ContourSettings.UserConfirmInitialValue", true).toBool();
  settings->setValue("ContourSettings.UserConfirmInitialValue", false);
  connect(this, &ModuleManager::stateDoneLoading, this,
          [settings, userConfirmInitialValue]() {
            settings->setValue("ContourSettings.UserConfirmInitialValue",
                               userConfirmInitialValue);
          });

  // High level game plan - construct some XML for ParaView, restore the
  // layouts, the views, links, etc. Once they are ready then restore the data
  // pipeline, using the nested layout to assure the order is correct.
  QJsonArray views = doc["views"].toArray();
  QJsonArray layouts = doc["layouts"].toArray();
  QJsonArray links = doc["links"].toArray();

  // ParaView must load all views and layouts first.
  pugi::xml_document document;
  auto pvxml = document.append_child("ParaView");
  auto pvState = pvxml.append_child("ServerManagerState");
  // Hardwire the ParaView version to avoid issues with a hardwired check for
  // versions less than 4.0.1 in the state version controller in ParaView.
  pvState.append_attribute("version").set_value("5.5.0");
  auto pvViews = pvState.append_child("ProxyCollection");
  pvViews.append_attribute("name").set_value("views");
  auto pvLayouts = pvState.append_child("ProxyCollection");
  pvLayouts.append_attribute("name").set_value("layouts");
  int numViews = 0, numLayouts = 0;

  // Check for a palette color
  if (doc.contains("paletteColor")) {
    auto paletteColorArray = doc["paletteColor"].toArray();
    double paletteColor[3] = { paletteColorArray[0].toDouble(),
                               paletteColorArray[1].toDouble(),
                               paletteColorArray[2].toDouble() };

    auto* pxm = ActiveObjects::instance().proxyManager();
    auto* paletteProxy = pxm->GetProxy("settings", "ColorPalette");
    vtkSMPropertyHelper(paletteProxy->GetProperty("BackgroundColor"))
      .Set(paletteColor, 3);
    paletteProxy->UpdateVTKObjects();
  }

  // First see if we have views, and unpack them.
  for (int i = 0; i < views.size(); ++i) {
    QJsonObject view = views[i].toObject();
    auto viewId = view["id"].toInt();
    auto proxyNode = pvState.append_child("Proxy");
    proxyNode.append_attribute("group").set_value("views");
    auto xmlName = view["xmlName"].toString("RenderView");
    proxyNode.append_attribute("type").set_value(
      xmlName.toStdString().c_str());
    proxyNode.append_attribute("id").set_value(viewId);
    proxyNode.append_attribute("servers").set_value(view["servers"].toInt());

    auto propNode = proxyNode.append_child("Property");
    createXmlProperty(propNode, "CenterOfRotation", viewId,
                      view["centerOfRotation"].toArray());
    // Let's do the camera now...
    QJsonObject camera = view["camera"].toObject();
    propNode = proxyNode.append_child("Property");
    createXmlProperty(propNode, "CameraFocalPoint", viewId,
                      camera["focalPoint"].toArray());

    if (view.contains("useColorPaletteForBackground")) {
      propNode = proxyNode.append_child("Property");
      createXmlProperty(propNode, "UseColorPaletteForBackground", viewId,
                        view["useColorPaletteForBackground"].toInt());
    }

    if (view.contains("backgroundColor")) {
      if (!view.contains("useColorPaletteForBackground")) {
        // Assume this is an older state file where we should turn this off
        propNode = proxyNode.append_child("Property");
        createXmlProperty(propNode, "UseColorPaletteForBackground", viewId, 0);
      }

      auto backgroundColor = view["backgroundColor"].toArray();
      // Restore the background color
      propNode = proxyNode.append_child("Property");
      createXmlProperty(propNode, "Background", viewId,
                        backgroundColor.at(0).toArray());

      // If we have more than one element, we have a gradient so also restore
      // Background2 and set UseGradientBackground.
      if (backgroundColor.size() > 1) {
        propNode = proxyNode.append_child("Property");
        createXmlProperty(propNode, "Background2", viewId,
                          backgroundColor.at(1).toArray());
        propNode = proxyNode.append_child("Property");
        createXmlProperty(propNode, "UseGradientBackground", viewId, 1);
      }
    }
    if (view.contains("isOrthographic")) {
      auto parallelProjection = view["isOrthographic"].toBool() ? 1 : 0;
      propNode = proxyNode.append_child("Property");
      createXmlProperty(propNode, "CameraParallelProjection", viewId,
                        parallelProjection);
    }
    if (view.contains("interactionMode")) {
      propNode = proxyNode.append_child("Property");
      auto modeString = view["interactionMode"].toString();
      int mode = 0; // default to 3D
      if (modeString == "2D") {
        mode = 1;
      } else if (modeString == "selection") {
        mode = 2;
      }
      createXmlProperty(propNode, "InteractionMode", viewId, mode);
    }

    // Create an entry in the views thing...
    pugi::xml_node viewSummary = pvViews.append_child("Item");
    viewSummary.append_attribute("id").set_value(viewId);
    viewSummary.append_attribute("name").set_value(
      QString("View%1").arg(++numViews).toStdString().c_str());
  }
  // Now the layouts - should only ever be one, but go through the motions...
  for (int i = 0; i < layouts.size(); ++i) {
    QJsonObject layout = layouts[i].toObject();
    auto layoutId = layout["id"].toInt();
    auto proxyNode = pvState.append_child("Proxy");
    proxyNode.append_attribute("group").set_value("misc");
    proxyNode.append_attribute("type").set_value("ViewLayout");
    proxyNode.append_attribute("id").set_value(layoutId);
    proxyNode.append_attribute("servers").set_value(layout["servers"].toInt());

    QJsonArray items = layout["items"].toArray();
    for (int j = 0; j < items.size(); ++j) {
      auto layoutNode = proxyNode.append_child("Layout");
      createXmlLayout(layoutNode, items[j].toArray());
    }

    // Create an entry in the layouts thing...
    auto layoutSummary = pvLayouts.append_child("Item");
    layoutSummary.append_attribute("id").set_value(layoutId);
    layoutSummary.append_attribute("name").set_value(
      QString("Layout%1").arg(++numLayouts).toStdString().c_str());
  }

  d->dir = stateDir;
  m_stateObject = doc;
  m_loadDataSources = loadDataSources;
  connect(pqApplicationCore::instance(),
          &pqApplicationCore::stateLoaded, this,
          &ModuleManager::onPVStateLoaded);
  // Set up call to ParaView to load state
  std::ostringstream stream;
  document.first_child().print(stream);

  // qDebug() << "\nPV XML:" << stream.str().c_str() << "\n";
  vtkNew<vtkPVXMLParser> parser;
  if (!parser->Parse(stream.str().c_str())) {
    d->LastStateLoadSuccess = false;
    return false;
  }
  pqActiveObjects* activeObjects = &pqActiveObjects::instance();
  pqServer* server = activeObjects->activeServer();

  pqApplicationCore::instance()->loadState(parser->GetRootElement(), server);
  // Clean up the state -- since the Qt slot call should be synchronous
  // it should be done before the code returns to here.
  disconnect(pqApplicationCore::instance(),
             &pqApplicationCore::stateLoaded, this,
             &ModuleManager::onPVStateLoaded);

  d->dir = QDir();
  m_stateObject = QJsonObject();

  // Restore the views to their state before the modules were added
  setViews(views);

  d->LastStateLoadSuccess = true;

  if (d->RemaningPipelinesToWaitFor == 0) {
    emit stateDoneLoading();
  }
  return true;
}

void ModuleManager::setViews(const QJsonArray& views)
{
  // This sets all views according to their settings in the view proxy.
  // It should be called after the views have been deserialized.
  for (int i = 0; i < views.size(); ++i) {
    auto view = views[i].toObject();
    auto viewProxy =
      vtkSMRenderViewProxy::SafeDownCast(lookupView(view["id"].toInt()));
    if (!viewProxy) {
      continue;
    }
    double tmp[3];
    auto camera = view["camera"].toObject();
    auto jsonArray = camera["position"].toArray();
    for (int j = 0; j < jsonArray.size(); ++j) {
      tmp[j] = jsonArray[j].toDouble();
    }
    vtkSMPropertyHelper(viewProxy, "CameraPosition").Set(tmp, 3);
    jsonArray = camera["focalPoint"].toArray();
    for (int j = 0; j < jsonArray.size(); ++j) {
      tmp[j] = jsonArray[j].toDouble();
    }
    vtkSMPropertyHelper(viewProxy, "CameraFocalPoint").Set(tmp, 3);
    jsonArray = camera["viewUp"].toArray();
    for (int j = 0; j < jsonArray.size(); ++j) {
      tmp[j] = jsonArray[j].toDouble();
    }
    vtkSMPropertyHelper(viewProxy, "CameraViewUp").Set(tmp, 3);
    vtkSMPropertyHelper(viewProxy, "CameraViewAngle")
      .Set(camera["viewAngle"].toDouble());
    vtkSMPropertyHelper(viewProxy, "EyeAngle")
      .Set(camera["EyeAngle"].toDouble());
    vtkSMPropertyHelper(viewProxy, "CameraParallelScale")
      .Set(camera["parallelScale"].toDouble());

    // restore axis grid visibility
    if (viewProxy->GetProperty("AxesGrid")) {
      vtkSMPropertyHelper axesGridProp(viewProxy, "AxesGrid");
      vtkSMProxy* proxy = axesGridProp.GetAsProxy();
      if (!proxy) {
        vtkSMSessionProxyManager* pxm = viewProxy->GetSessionProxyManager();
        proxy = pxm->NewProxy("annotations", "GridAxes3DActor");
        axesGridProp.Set(proxy);
        proxy->Delete();
      }
      vtkSMPropertyHelper(proxy, "Visibility")
        .Set(view["axesGridVisibility"].toBool() ? 1 : 0);
      proxy->UpdateVTKObjects();
    }
    if (view.contains("centerAxesVisible")) {
      vtkSMPropertyHelper(viewProxy, "CenterAxesVisibility")
        .Set(view["centerAxesVisible"].toBool() ? 1 : 0);
    }
    if (view.contains("orientationAxesVisible")) {
      vtkSMPropertyHelper(viewProxy, "OrientationAxesVisibility")
        .Set(view["orientationAxesVisible"].toBool() ? 1 : 0);
    }
    viewProxy->UpdateVTKObjects();
  }
  // force the view menu to update its state based on the settings we have
  // restored to the view
  ActiveObjects::instance().viewChanged(ActiveObjects::instance().activeView());
}

bool ModuleManager::lastLoadStateSucceeded()
{
  return d->LastStateLoadSuccess;
}

void ModuleManager::onPVStateLoaded(vtkPVXMLElement*,
                                    vtkSMProxyLocator* locator)
{
  auto pxm = ActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  // Populate the view id map, needed to create modules with the restored views.
  d->ViewIdMap.clear();
  if (m_stateObject["views"].isArray()) {
    auto viewArray = m_stateObject["views"].toArray();
    for (int i = 0; i < viewArray.size(); ++i) {
      auto view = viewArray[i].toObject();
      auto viewId = view["id"].toInt();
      d->ViewIdMap[viewId] =
        vtkSMViewProxy::SafeDownCast(locator->LocateProxy(viewId));
    }
  }

  // Load up all of the data sources.
  if (m_loadDataSources && m_stateObject["dataSources"].isArray()) {
    auto dataSources = m_stateObject["dataSources"].toArray();
    loadDataSources(dataSources);
  }

  m_isDeserializing = false;

  if (!executePipelinesOnLoad() || d->RemaningPipelinesToWaitFor == 0) {
    // If there are no pipelines left to wait for, most likely, the
    // pipelines finished before deserialization finished. Go ahead and
    // emit the signal.
    emit stateDoneLoading();
  }
}

void ModuleManager::incrementPipelinesToWaitFor()
{
  ++d->RemaningPipelinesToWaitFor;
}

void ModuleManager::onPipelineFinished()
{
  --d->RemaningPipelinesToWaitFor;

  // It could still be deserializing if the pipeline finishes early.
  // In this case, the signal will be emitted after deserialization
  // finishes.
  if (d->RemaningPipelinesToWaitFor == 0 && !m_isDeserializing) {
    emit stateDoneLoading();
  }
  if (d->RemaningPipelinesToWaitFor <= 0) {
    Pipeline* p = qobject_cast<Pipeline*>(sender());
    disconnect(p, &Pipeline::finished, this,
               &ModuleManager::onPipelineFinished);
  }
}

void ModuleManager::executePipelinesOnLoad(bool execute)
{
  d->ExecutePipelinesOnLoad = execute;
}

bool ModuleManager::executePipelinesOnLoad() const
{
  return d->ExecutePipelinesOnLoad;
}

void ModuleManager::onViewRemoved(pqView* view)
{
  Q_ASSERT(view);
  auto viewProxy = view->getViewProxy();
  QList<Module*> modules;
  foreach (Module* module, d->Modules) {
    if (module->view() == viewProxy) {
      modules.push_back(module);
    }
  }
  foreach (Module* module, modules) {
    removeModule(module);
  }
}

void ModuleManager::render()
{
  auto view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}

void ModuleManager::updateClientSideView()
{
  auto* view = ActiveObjects::instance().activeView();
  if (!view) {
    return;
  }

  auto* clientView = vtkPVRenderView::SafeDownCast(view->GetClientSideView());
  if (!clientView) {
    return;
  }

  clientView->Update();
}

vtkSMViewProxy* ModuleManager::lookupView(int id)
{
  return d->ViewIdMap.value(id);
}

bool ModuleManager::hasDataSources()
{
  d->removeInvalidDataSources();
  return !d->DataSources.empty();
}

bool ModuleManager::hasMoleculeSources()
{
  return !d->MoleculeSources.empty();
}

void ModuleManager::loadDataSources(const QJsonArray& dataSourcesArray)
{
  // Data source arguments in operators make it so that we have to
  // load in the data sources in a proper order, or else the arguments
  // will not be able to be resolved for the operator to use.

  auto dataSources = toObjectList(dataSourcesArray);
  QMap<QString, DataSource*> alreadyLoaded;

  // This local load data source function ensures we load dependencies
  // before we load in the requested data source. It also ensures that
  // we do not load in any data sources twice.
  std::function<void(QJsonObject ds)> localLoadDataSource;
  localLoadDataSource = [&dataSources, &alreadyLoaded, &localLoadDataSource,
                         this](QJsonObject ds) {
    auto id = ds.value("id").toString();
    if (alreadyLoaded.contains(id)) {
      // Already have this one
      return;
    }

    // First, see if it has any dependencies.
    auto deps = d->dataSourceDependencies(ds);

    // Load the dependencies first.
    for (auto& dep : deps) {
      // Load the root data source of this dependency
      auto rootObj = d->rootDataSourceDependency(dep, dataSources);
      localLoadDataSource(rootObj);

      // If it is a child dependency, wait for the pipeline to finish
      if (d->isChildDataSourceDependency(dep, dataSources)) {
        auto rootId = rootObj.value("id").toString();
        auto* depDataSource = alreadyLoaded[rootId];
        if (depDataSource->pipeline()->isRunning()) {
          QEventLoop loop;
          connect(depDataSource->pipeline(), &Pipeline::finished, &loop,
                  &QEventLoop::quit);
          loop.exec();
        };
      }
    }

    // Now that the dependencies are loaded, we can load the data source...
    alreadyLoaded[id] = loadDataSource(ds);
  };

  for (auto& ds : dataSources) {
    localLoadDataSource(ds);
  }
}

DataSource* ModuleManager::loadDataSource(QJsonObject& dsObject)
{
  QJsonObject options;
  options["defaultModules"] = false;
  options["addToRecent"] = false;
  options["child"] = false;
  d->absoluteFilePaths(dsObject);

  QStringList fileNames;
  if (dsObject.contains("reader")) {
    auto reader = dsObject["reader"].toObject();

    if (reader.contains("fileNames")) {
      foreach (const QJsonValue& value, reader["fileNames"].toArray()) {
        // Verify the file exists before adding it to the list.
        if (QFileInfo::exists(value.toString())) {
          fileNames << value.toString();
        } else {
          // If the file cannot be found in the path relative to the state
          // file, make another attempt to locate it in the same directory
          QString altLocation =
            d->dir.absoluteFilePath(QFileInfo(value.toString()).fileName());
          if (QFileInfo::exists(altLocation)) {
            fileNames << altLocation;
          } else {
            qCritical() << "File" << value.toString() << "not found, skipping.";
          }
        }
      }
      reader["fileNames"] = QJsonArray::fromStringList(fileNames);
    } else {
      qCritical() << "Unable to locate file name(s).";
    }
    if (reader.contains("name")) {
      options["reader"] = reader;
    }
    if (reader.contains("subsampleSettings")) {
      options["subsampleSettings"] = reader["subsampleSettings"];
    }
    if (reader.contains("tvh5NodePath")) {
      options["tvh5NodePath"] = reader["tvh5NodePath"];
    }
  }

  if (!options.contains("subsampleSettings")) {
    if (dsObject.contains("subsampleSettings")) {
      // Make sure subsample settings get communicated to the readers
      options["subsampleSettings"] = dsObject["subsampleSettings"];
    } else {
      // Never prompt the user for subsample setting when loading state
      // files. This will prevent the prompt from happening.
      options["subsampleSettings"] = QJsonObject();
    }
  }

  // TODO: Legacy state restoration — this path creates SourceNodes via
  // LoadDataReaction but cannot fully deserialize DataSource state.
  // Full serialization support for the new pipeline is a separate concern.
  pipeline::SourceNode* sourceNode = nullptr;
  if (dsObject.find("sourceInformation") != dsObject.end()) {
    DataSource* ds = PythonGeneratedDatasetReaction::createDataSource(
      dsObject["sourceInformation"].toObject());
    if (ds) {
      auto* sn = new pipeline::SourceNode();
      sn->setLabel(ds->label());
      sn->addOutput("volume", pipeline::PortType::ImageData);
      vtkSmartPointer<vtkImageData> img = ds->imageData();
      auto vol = std::make_shared<pipeline::VolumeData>(img);
      vol->setLabel(ds->label());
      sn->setOutputData(
        "volume",
        pipeline::PortData(vol, pipeline::PortType::ImageData));
      LoadDataReaction::sourceNodeAdded(sn, false, false);
      sourceNode = sn;
      delete ds;
    }
  } else if (fileNames.size() > 0) {
    sourceNode = LoadDataReaction::loadData(fileNames, options);
  } else {
    qCritical() << "Files not found on disk for data source, check paths.";
  }

  Q_UNUSED(sourceNode);
  // TODO: The following legacy code is not yet migrated:
  // - Pipeline::finished connection for operators
  // - dataSource->deserialize(dsObject)
  // - setPersistenceState for transient sources
  // - setActiveDataSource

  return nullptr;
}

void ModuleManager::setMostRecentStateFile(const QString& s)
{
  m_mostRecentStateFile = s;
  emit mostRecentStateFileChanged(m_mostRecentStateFile);
}

} // namespace tomviz
