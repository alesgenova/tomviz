/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DeleteDataReaction.h"

#include "ActiveObjects.h"

// TODO: Re-implement delete data using the new pipeline API.
// The old implementation relied on ActiveObjects::activeDataSource(),
// DataSource, Pipeline, and ModuleManager which have been removed or changed.

namespace tomviz {

DeleteDataReaction::DeleteDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  parentAction()->setEnabled(false);
}

void DeleteDataReaction::updateEnableState()
{
  parentAction()->setEnabled(false);
}

void DeleteDataReaction::onTriggered()
{
  // TODO: implement with new pipeline
}

void DeleteDataReaction::deleteDataSource(DataSource*)
{
  // TODO: implement with new pipeline
}

void DeleteDataReaction::activeDataSourceChanged()
{
  // TODO: implement with new pipeline
}

} // end of namespace tomviz
