/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RecentFilesMenu.h"

#include "ActiveObjects.h"
#include "LoadDataReaction.h"
#include "SaveLoadStateReaction.h"
#include "SaveLoadTemplateReaction.h"
#include "Utilities.h"

#include "pipeline/Node.h"
#include "pipeline/SourceNode.h"

#include <pqSettings.h>

#include <QDebug>
#include <QMenu>
#include <QMessageBox>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <string>

namespace tomviz {

static const int MAX_ITEMS = 10;

namespace {

QJsonObject loadSettings()
{
  // Load the recent files from our saved state.
  auto settings = pqApplicationCore::instance()->settings();
  // Remove a key that can interfere/double count.
  settings->remove("RecentlyUsedResourcesList");
  auto recent = settings->value("recentFiles").toByteArray();
  auto doc = QJsonDocument::fromJson(recent);
  if (!doc.isNull() && doc.isObject()) {
    return doc.object();
  } else {
    QJsonObject json;
    json["readers"] = QJsonArray();
    json["molecules"] = QJsonArray();
    json["states"] = QJsonArray();
    return QJsonObject();
  }
}

void saveSettings(QJsonObject json)
{
  // Save recent file list to our saved state, prune lists to be <= 10 entries.
  QJsonArray readers;
  if (json.contains("readers") && json["readers"].isArray()) {
    readers = json["readers"].toArray();
  }
  QJsonArray states;
  if (json.contains("states") && json["states"].isArray()) {
    states = json["states"].toArray();
  }
  QJsonArray molecules;
  if (json.contains("molecules") && json["molecules"].isArray()) {
    molecules = json["molecules"].toArray();
  }
  QJsonArray templates;
  if (json.contains("templates") && json["templates"].isArray()) {
    templates = json["templates"].toArray();
  }
  auto prune = [](QJsonArray& arr) {
    while (arr.size() > MAX_ITEMS) {
      arr.removeLast();
    }
  };
  prune(readers);
  prune(states);
  prune(molecules);
  prune(templates);
  json["readers"] = readers;
  json["states"] = states;
  json["molecules"] = molecules;
  json["templates"] = templates;
  QJsonDocument doc(json);

  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("recentFiles", doc.toJson(QJsonDocument::Compact));
  // Remove a key that can interfere/double count.
  settings->remove("RecentlyUsedResourcesList");
}

} // namespace

RecentFilesMenu::RecentFilesMenu(QMenu& menu, QObject* p) : QObject(p)
{
  connect(&menu, &QMenu::aboutToShow, this, &RecentFilesMenu::aboutToShowMenu);
}

RecentFilesMenu::~RecentFilesMenu() = default;

void RecentFilesMenu::pushDataReader(pipeline::SourceNode* source)
{
  // Add non-proxy based readers separately.
  auto settings = loadSettings();
  auto readerList = settings["readers"].toArray();

  // Get reader properties and file names from node properties
  QVariantMap readerProps =
    source->property("readerProperties").toMap();
  QJsonObject readerJson = QJsonObject::fromVariantMap(readerProps);
  QStringList fileNames = source->property("fileNames").toStringList();

  if (fileNames.size() < 1) {
    return;
  }

  readerJson["fileNames"] = QJsonArray::fromStringList(fileNames);

  // Remove the file if it is already in the list
  for (int i = readerList.size() - 1; i >= 0; --i) {
    if (readerList[i].toObject()["fileNames"].toArray()[0] ==
        readerJson["fileNames"].toArray()[0]) {
      readerList.removeAt(i);
    }
  }
  readerList.push_front(readerJson);
  settings["readers"] = readerList;
  saveSettings(settings);
}

void RecentFilesMenu::pushStateFile(const QString& fileName)
{
  auto settings = loadSettings();
  auto stateList = settings["states"].toArray();
  QJsonObject stateJson;
  stateJson["fileName"] = fileName;
  // Remove the file if it is already in the list
  for (int i = stateList.size() - 1; i >= 0; --i) {
    if (stateList[i].toObject()["fileName"] == stateJson["fileName"]) {
      stateList.removeAt(i);
    }
  }
  stateList.push_front(stateJson);
  settings["states"] = stateList;
  saveSettings(settings);
}

void RecentFilesMenu::pushTemplateFile(const QString& fileName)
{
  auto settings = loadSettings();
  auto templateList = settings["templates"].toArray();
  QJsonObject templateJson;
  templateJson["fileName"] = fileName;
  for (int i = templateList.size() - 1; i >= 0; --i) {
    if (templateList[i].toObject()["fileName"] == templateJson["fileName"]) {
      templateList.removeAt(i);
    }
  }
  templateList.push_front(templateJson);
  settings["templates"] = templateList;
  saveSettings(settings);
}

void RecentFilesMenu::aboutToShowMenu()
{
  auto menu = qobject_cast<QMenu*>(sender());
  Q_ASSERT(menu);
  menu->clear();
  menu->setToolTipsVisible(true);

  auto json = loadSettings();

  // There are no entries, make it clear in the recent files menu.
  if (!json.contains("readers") && !json.contains("states")) {
    auto actn = menu->addAction("Empty");
    actn->setEnabled(false);
    return;
  }

  // We have something, let's populate the recent files and/or state files.
  if (json["readers"].toArray().size() > 0) {
    menu->addAction("Data files")->setEnabled(false);
  }

  int index = 0;

  foreach (QJsonValue file, json["readers"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      auto fileNamesArray = object["fileNames"].toArray();
      QStringList fileNames;
      foreach (file, fileNamesArray) {
        fileNames << file.toString("<bug>");
      }
      QString label = fileNames[0];
      QString toolTip;
      // Truncate the number of files in the tooltip to 15
      const auto maxEntries = 15;
      if (fileNames.size() > maxEntries) {
        toolTip = (fileNames.mid(0, maxEntries) << QString("...")).join("\n");
      } else {
        toolTip = fileNames.join("\n");
      }
      auto actn = menu->addAction(QIcon(":/icons/pqInspect.png"), label);
      actn->setToolTip(toolTip);
      actn->setData(index);
      connect(actn, &QAction::triggered, [this, actn, fileNames]() {
        dataSourceTriggered(actn, fileNames);
      });
      ++index;
    }
  }

  if (json["states"].toArray().size() > 0) {
    menu->addAction("State files")->setEnabled(false);
  }

  foreach (QJsonValue file, json["states"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      auto actn = menu->addAction(QIcon(":/icons/tomviz.png"),
                                  object["fileName"].toString("<bug>"));
      actn->setData(object["fileName"].toString("<bug>"));
      connect(actn, &QAction::triggered, this, &RecentFilesMenu::stateTriggered);
    }
  }

  if (json["templates"].toArray().size() > 0) {
    menu->addAction("Template files")->setEnabled(false);
  }

  foreach (QJsonValue file, json["templates"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      auto actn = menu->addAction(QIcon(":/icons/tomviz.png"),
                                  object["fileName"].toString("<bug>"));
      actn->setData(object["fileName"].toString("<bug>"));
      connect(actn, &QAction::triggered, this,
              &RecentFilesMenu::templateTriggered);
    }
  }
}

void RecentFilesMenu::dataSourceTriggered(QAction* actn, QStringList fileNames)
{
  // Check the files actually exists, remove the recent entry if not.
  int missingIdx = -1;
  for (auto i = 0; i < fileNames.size(); ++i) {
    auto file = fileNames[i];
    if (!QFileInfo::exists(file)) {
      QMessageBox::warning(tomviz::mainWidget(), "Error",
                           QString("The file '%1' does not exist").arg(file));
      missingIdx = i;
      break;
    }
  }
  if (missingIdx != -1) {
    int index = actn->data().toInt();
    auto json = loadSettings();
    auto readers = json["readers"].toArray();
    readers.removeAt(index);
    json["readers"] = readers;
    saveSettings(json);
    return;
  }

  LoadDataReaction::loadData(fileNames);
}

void RecentFilesMenu::stateTriggered()
{
  auto actn = qobject_cast<QAction*>(sender());
  Q_ASSERT(actn);

  QString fileName = actn->data().toString();

  if (QFileInfo::exists(actn->iconText())) {
    if (SaveLoadStateReaction::loadState(fileName)) {
      // the above call will ensure that the file name moves to top of the list
      // since it calls pushStateFile() on success.
      return;
    }
  } else {
    // If the user tried to open a recent file that no longer exists, remove
    // it from the recent files.
    QMessageBox::warning(
      tomviz::mainWidget(), "Error",
      QString("The file '%1' does not exist").arg(actn->iconText()));
  }

  // Remove the item from the recent state files list.
  auto json = loadSettings();
  if (json["states"].isArray()) {
    auto states = json["states"].toArray();
    for (int i = 0; i < states.size(); ++i) {
      if (states[i].toObject()["fileName"].toString() == fileName) {
        states.removeAt(i);
      }
    }
    saveSettings(json);
  }
}

void RecentFilesMenu::templateTriggered()
{
  auto actn = qobject_cast<QAction*>(sender());
  Q_ASSERT(actn);

  QString fileName = actn->data().toString();

  if (QFileInfo::exists(fileName)) {
    if (SaveLoadTemplateReaction::loadTemplate(fileName)) {
      // Move the entry back to the top of the list.
      pushTemplateFile(fileName);
      return;
    }
  } else {
    QMessageBox::warning(
      tomviz::mainWidget(), "Error",
      QString("The file '%1' does not exist").arg(fileName));
  }

  auto json = loadSettings();
  if (json["templates"].isArray()) {
    auto templates = json["templates"].toArray();
    for (int i = 0; i < templates.size(); ++i) {
      if (templates[i].toObject()["fileName"].toString() == fileName) {
        templates.removeAt(i);
      }
    }
    json["templates"] = templates;
    saveSettings(json);
  }
}
} // namespace tomviz
