/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DuplicateModuleReaction.h"

#include "ActiveObjects.h"
#include "legacy/modules/Module.h"
#include "legacy/modules/ModuleFactory.h"
#include "legacy/modules/ModuleManager.h"

#include <QJsonObject>

namespace tomviz {

DuplicateModuleReaction::DuplicateModuleReaction(QAction* action)
  : pqReaction(action)
{
  // TODO: migrate to new pipeline
  // was: connected to ActiveObjects::moduleChanged to update enable state
  parentAction()->setEnabled(false);
}

void DuplicateModuleReaction::updateEnableState()
{
  // TODO: migrate to new pipeline
  // was: enabled when ActiveObjects::instance().activeModule() != nullptr
  parentAction()->setEnabled(false);
}

void DuplicateModuleReaction::onTriggered()
{
  // TODO: migrate to new pipeline
  // was: auto module = ActiveObjects::instance().activeModule();
  Module* module = nullptr;
  if (!module) {
    return;
  }
  auto dataSource = module->dataSource();
  auto operatorResult = module->operatorResult();
  auto moleculeSource = module->moleculeSource();
  auto view = ActiveObjects::instance().activeView();
  auto moduleType = ModuleFactory::moduleType(module);
  // Copy the module
  Module* copy;
  if (ModuleFactory::moduleApplicable(moduleType, dataSource, view)) {
    copy = ModuleFactory::createModule(moduleType, dataSource, view);
  } else if (ModuleFactory::moduleApplicable(moduleType, moleculeSource,
                                             view)) {
    copy = ModuleFactory::createModule(moduleType, moleculeSource, view);
  } else {
    copy = ModuleFactory::createModule(moduleType, operatorResult, view);
  }

  if (copy) {
    // Copy its settings
    QJsonObject json = module->serialize();
    copy->deserialize(json);
    ModuleManager::instance().addModule(copy);
  }
}

} // end namespace tomviz
