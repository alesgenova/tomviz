/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonNodeBackend.h"

#include "InputPort.h"
#include "Node.h"
#include "OutputPort.h"
#include "PythonNodeUtils.h"
#include "PythonNodeWrapper.h"
#include "data/VolumeData.h"

#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "pybind11/PybindVTKTypeCaster.h"
#pragma pop_macro("slots")

#include <vtkImageData.h>
#include <vtkMolecule.h>
#include <vtkNew.h>
#include <vtkPythonUtil.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>

#include <QJsonDocument>

PYBIND11_VTK_TYPECASTER(vtkImageData)

namespace py = pybind11;

namespace tomviz {
namespace pipeline {

namespace {

// Names of the user-facing Python base classes in tomviz.nodes. The
// backend imports these to hand to PythonNodeUtils::findNodeClass when
// dispatching to the user's class.
constexpr const char* kSourceBaseAttr = "SourceNode";
constexpr const char* kTransformBaseAttr = "TransformNode";

/// Convert a PortData payload into a Python object suitable for the
/// inputs dict. ImageData/Volume/TiltSeries get wrapped in a fresh
/// internal_dataset.Dataset (around a deep copy of the input vtkImageData, so
/// in-place mutations by the user's transform don't corrupt the
/// upstream port's payload). Tables/molecules pass through as raw
/// vtkObjects via vtkPythonUtil.
py::object portDataToPython(const PortData& data, py::object datasetCls)
{
  if (!data.isValid()) {
    return py::none();
  }
  PortType type = data.type();
  if (isVolumeType(type)) {
    auto vol = data.value<VolumeDataPtr>();
    if (!vol || !vol->isValid()) {
      return py::none();
    }
    vtkNew<vtkImageData> copy;
    copy->DeepCopy(vol->imageData());
    return datasetCls(py::cast(static_cast<vtkImageData*>(copy.Get()),
                               py::return_value_policy::reference));
  }
  if (type == PortType::Table) {
    auto sp = data.value<vtkSmartPointer<vtkTable>>();
    if (!sp) {
      return py::none();
    }
    return py::reinterpret_steal<py::object>(
      vtkPythonUtil::GetObjectFromPointer(sp.GetPointer()));
  }
  if (type == PortType::Molecule) {
    auto sp = data.value<vtkSmartPointer<vtkMolecule>>();
    if (!sp) {
      return py::none();
    }
    return py::reinterpret_steal<py::object>(
      vtkPythonUtil::GetObjectFromPointer(sp.GetPointer()));
  }
  return py::none();
}

} // namespace

PythonNodeBackend::PythonNodeBackend() = default;

void PythonNodeBackend::setJSONDescription(const QString& json)
{
  m_jsonDescription = json;
  parseDescription();
}

QString PythonNodeBackend::jsonDescription() const
{
  return m_jsonDescription;
}

void PythonNodeBackend::setScript(const QString& script)
{
  m_script = script;
}

QString PythonNodeBackend::scriptSource() const
{
  return m_script;
}

QString PythonNodeBackend::operatorName() const { return m_operatorName; }
QString PythonNodeBackend::defaultLabel() const { return m_defaultLabel; }
QString PythonNodeBackend::descriptionText() const { return m_description; }
QString PythonNodeBackend::helpText() const { return m_help; }
QString PythonNodeBackend::customWidgetID() const { return m_customWidgetID; }
bool PythonNodeBackend::supportsCancel() const { return m_supportsCancel; }
bool PythonNodeBackend::supportsComplete() const { return m_supportsComplete; }
bool PythonNodeBackend::isTransformShape() const { return !m_inputs.isEmpty(); }

QString PythonNodeBackend::externalPythonEnvPath() const
{
  return m_externalPythonEnvPath;
}

void PythonNodeBackend::setParameter(const QString& name,
                                     const QVariant& value)
{
  m_parameters[name] = value;
}

QVariant PythonNodeBackend::parameter(const QString& name) const
{
  return m_parameters.value(name);
}

QMap<QString, QVariant> PythonNodeBackend::parameters() const
{
  return m_parameters;
}

QMap<QString, ParameterBinding> PythonNodeBackend::parameterBindings() const
{
  return m_parameterBindings;
}

QStringList PythonNodeBackend::inputNames() const
{
  QStringList names;
  for (const auto& p : m_inputs) {
    names.append(p.name);
  }
  return names;
}

QStringList PythonNodeBackend::outputNames() const
{
  QStringList names;
  for (const auto& p : m_outputs) {
    names.append(p.name);
  }
  return names;
}

QString PythonNodeBackend::primaryOutputName() const
{
  return m_outputs.isEmpty() ? QString() : m_outputs.first().name;
}

void PythonNodeBackend::parseDescription()
{
  m_operatorName.clear();
  m_defaultLabel.clear();
  m_description.clear();
  m_help.clear();
  m_customWidgetID.clear();
  m_supportsCancel = false;
  m_supportsComplete = false;
  m_externalPythonEnvPath.clear();
  m_inputs.clear();
  m_outputs.clear();
  m_parameters.clear();
  m_parameterTypes.clear();
  m_enumOptions.clear();
  m_parameterBindings.clear();

  QJsonDocument doc = QJsonDocument::fromJson(m_jsonDescription.toUtf8());
  if (!doc.isObject()) {
    return;
  }
  QJsonObject obj = doc.object();
  m_parameterBindings = parseParameterBindings(obj);

  m_operatorName = obj.value(QStringLiteral("name")).toString();
  m_defaultLabel = obj.value(QStringLiteral("label")).toString();
  m_description = obj.value(QStringLiteral("description")).toString();
  m_help = obj.value(QStringLiteral("help")).toString();
  m_customWidgetID = obj.value(QStringLiteral("widget")).toString();
  m_supportsCancel =
    obj.value(QStringLiteral("supportsCancel")).toBool(false);
  m_supportsComplete =
    obj.value(QStringLiteral("supportsComplete")).toBool(false);
  m_externalPythonEnvPath =
    obj.value(QStringLiteral("tomviz_pipeline_env")).toString();

  // inputs / outputs are arrays of {name, type[, persistent]}. Missing
  // section → empty list (per the agreed schema-v2 convention).
  for (const auto& v : obj.value(QStringLiteral("inputs")).toArray()) {
    QJsonObject entry = v.toObject();
    PortSpec spec;
    spec.name = entry.value(QStringLiteral("name")).toString();
    spec.type =
      portTypeFromString(entry.value(QStringLiteral("type")).toString());
    if (!spec.name.isEmpty() && spec.type != PortType::None) {
      m_inputs.append(spec);
    }
  }
  for (const auto& v : obj.value(QStringLiteral("outputs")).toArray()) {
    QJsonObject entry = v.toObject();
    PortSpec spec;
    spec.name = entry.value(QStringLiteral("name")).toString();
    spec.type =
      portTypeFromString(entry.value(QStringLiteral("type")).toString());
    if (entry.contains(QStringLiteral("persistent"))) {
      spec.persistent =
        entry.value(QStringLiteral("persistent")).toBool(false);
      spec.persistentSpecified = true;
    }
    if (!spec.name.isEmpty() && spec.type != PortType::None) {
      m_outputs.append(spec);
    }
  }

  // parameters: same shape and coercion semantics as schema-v1, so the
  // shared PythonNodeUtils helpers handle int/double/enum without
  // Qt6's QJsonValue→QVariant<double> collapse breaking type-sensitive
  // operators.
  for (const auto& v : obj.value(QStringLiteral("parameters")).toArray()) {
    QJsonObject param = v.toObject();
    QString name = param.value(QStringLiteral("name")).toString();
    QString type = param.value(QStringLiteral("type")).toString();
    QJsonValue defaultVal = param.value(QStringLiteral("default"));
    if (name.isEmpty()) {
      continue;
    }
    m_parameterTypes[name] = type;

    if (type == QLatin1String("enumeration")) {
      QJsonArray options = param.value(QStringLiteral("options")).toArray();
      m_enumOptions[name] = options;
      QVariant resolved =
        PythonNodeUtils::resolveEnumValue(defaultVal, options);
      if (resolved.isValid()) {
        m_parameters[name] = resolved;
        continue;
      }
    }

    QVariant value =
      PythonNodeUtils::coerceJsonByDeclaredType(defaultVal, type);
    if (!value.isValid()) {
      // Complex / unknown declared type with no explicit default: let
      // the user's Python default win. With an explicit default fall
      // back to QJsonValue's own conversion (preserves string / list).
      if (defaultVal.isUndefined() || defaultVal.isNull()) {
        continue;
      }
      value = defaultVal.toVariant();
    }
    m_parameters[name] = value;
  }
}

void PythonNodeBackend::applyDescription(AddInputFn addInput,
                                         AddOutputFn addOutput)
{
  if (addInput) {
    for (const auto& spec : m_inputs) {
      addInput(spec.name, spec.type);
    }
  }
  if (addOutput) {
    for (const auto& spec : m_outputs) {
      OutputPort* port = addOutput(spec.name, spec.type);
      if (port && spec.persistentSpecified) {
        // Honor an explicit "persistent" flag from the operator JSON.
        // When omitted, leave whatever default the host node-class
        // installed (SourceNode → InMemory persistent; TransformNode →
        // OnDisk persistent during the temporary rollout).
        port->setPersistent(spec.persistent);
      }
    }
  }
}

QJsonObject PythonNodeBackend::serializeInto(QJsonObject base) const
{
  base[QStringLiteral("description")] = m_jsonDescription;
  base[QStringLiteral("script")] = m_script;
  if (!m_parameters.isEmpty()) {
    base[QStringLiteral("arguments")] =
      QJsonObject::fromVariantMap(m_parameters);
  }
  return base;
}

void PythonNodeBackend::applySerializedFields(const QJsonObject& json,
                                              AddInputFn addInput,
                                              AddOutputFn addOutput)
{
  if (json.contains(QStringLiteral("description"))) {
    setJSONDescription(json.value(QStringLiteral("description")).toString());
  }
  if (json.contains(QStringLiteral("script"))) {
    setScript(json.value(QStringLiteral("script")).toString());
  }
  applyDescription(std::move(addInput), std::move(addOutput));

  auto args = json.value(QStringLiteral("arguments")).toObject();
  for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
    const QString& key = it.key();
    const QString declType = m_parameterTypes.value(key);
    QVariant qv;
    if (declType == QLatin1String("enumeration")) {
      qv = PythonNodeUtils::resolveEnumValue(
        it.value(), m_enumOptions.value(key));
    }
    if (!qv.isValid()) {
      qv = PythonNodeUtils::coerceJsonByDeclaredType(it.value(), declType);
    }
    if (!qv.isValid()) {
      qv = it.value().toVariant();
    }
    m_parameters[key] = qv;
  }
}

QMap<QString, PortData> PythonNodeBackend::runTransform(
  Node* host, const QMap<QString, PortData>& inputs)
{
  return runImpl(host, inputs, /*isSource=*/false);
}

QMap<QString, PortData> PythonNodeBackend::runSource(Node* host)
{
  return runImpl(host, {}, /*isSource=*/true);
}

QMap<QString, PortData> PythonNodeBackend::runImpl(
  Node* host, const QMap<QString, PortData>& inputs, bool isSource)
{
  QMap<QString, PortData> result;
  if (!host) {
    return result;
  }

  // Ensure the embedded interpreter is alive. Never finalize — C
  // extension modules like numpy can't be re-loaded after
  // finalize_interpreter().
  if (!Py_IsInitialized()) {
    py::initialize_interpreter();
  }

  try {
    py::gil_scoped_acquire gil;

    // Some user scripts may reach into tomviz.utils.* without
    // importing it (legacy carry-over).
    py::module_::import("tomviz.utils");

    py::module_ datasetMod = py::module_::import("tomviz.internal_dataset");
    py::object datasetCls = datasetMod.attr("Dataset");

    py::module_ nodesMod = py::module_::import("tomviz.nodes");
    py::object baseClass =
      nodesMod.attr(isSource ? kSourceBaseAttr : kTransformBaseAttr);

    py::object scriptModule =
      PythonNodeUtils::loadScriptAsModule(m_operatorName, m_script);

    py::object userClass =
      PythonNodeUtils::findNodeClass(scriptModule, baseClass);
    if (userClass.is_none()) {
      qWarning("PythonNodeBackend: no %s subclass found in script",
               isSource ? "SourceNode" : "TransformNode");
      return result;
    }

    // Instantiate following the operator-class pattern: __new__ → set
    // _operator_wrapper → __init__. Lets the user's __init__ access
    // self.progress / self.canceled / self.completed.
    py::object wrapper =
      createNodeWrapper(host, primaryOutputName());
    py::object instance = userClass.attr("__new__")(userClass);
    instance.attr("_operator_wrapper") = wrapper;
    userClass.attr("__init__")(instance);

    // Build kwargs from current parameters.
    py::dict kwargs;
    for (auto it = m_parameters.constBegin();
         it != m_parameters.constEnd(); ++it) {
      kwargs[py::str(it.key().toStdString())] =
        PythonNodeUtils::qvariantToPython(it.value());
    }

    py::object pyResult;
    if (isSource) {
      pyResult = instance.attr("produce")(**kwargs);
    } else {
      // Convert each input PortData to the right Python representation.
      // Volume-shaped inputs get a deep-copied internal_dataset.Dataset wrapper
      // so user mutation doesn't leak upstream.
      py::dict inputsDict;
      for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
        inputsDict[py::str(it.key().toStdString())] =
          portDataToPython(it.value(), datasetCls);
      }
      pyResult =
        instance.attr("transform")(inputsDict, **kwargs);
    }

    if (host->isCanceled()) {
      return result;
    }

    if (pyResult.is_none() || !py::isinstance<py::dict>(pyResult)) {
      // None is the documented return for "cancellation or error" per
      // tomviz.nodes — the user's transform/produce returned without
      // producing outputs. Any other non-dict return is treated the
      // same way. The caller's port-empty check transitions the node
      // to Failed.
      return result;
    }

    // Pull each declared output by name. Unknown / missing keys are
    // ignored (the caller will detect missing outputs via the
    // node-level check that PortData was set on every output port).
    py::dict outDict = pyResult.cast<py::dict>();
    for (const auto& spec : m_outputs) {
      OutputPort* port = host->outputPort(spec.name);
      if (!port) {
        continue;
      }
      std::string key = spec.name.toStdString();
      if (!outDict.contains(key)) {
        continue;
      }
      py::object payload = outDict[py::str(key)];
      PortData pd =
        PythonNodeUtils::pythonValueToPortData(payload, port);
      if (pd.isValid()) {
        result[spec.name] = pd;
      }
    }
  } catch (const py::error_already_set& e) {
    qWarning("PythonNodeBackend Python error: %s", e.what());
  } catch (const std::exception& e) {
    qWarning("PythonNodeBackend error: %s", e.what());
  }

  return result;
}

} // namespace pipeline
} // namespace tomviz
