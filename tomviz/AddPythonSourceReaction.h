/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAddPythonSourceReaction_h
#define tomvizAddPythonSourceReaction_h

#include <pqReaction.h>

#include <QString>

namespace tomviz {

/// Menu reaction for adding a schema-v2 Python source node to the
/// pipeline. Pops up a modal dialog whose parameter form is built
/// from the operator JSON description (via ParameterInterfaceBuilder),
/// constructs a PythonSource with the chosen values, and hands it to
/// LoadDataReaction::sourceNodeAdded so the standard volume-loading
/// machinery (sink group, default modules, color map) kicks in.
class AddPythonSourceReaction : public pqReaction
{
  Q_OBJECT

public:
  AddPythonSourceReaction(QAction* parentObject, const QString& script,
                          const QString& json);

protected:
  void onTriggered() override;

private:
  Q_DISABLE_COPY(AddPythonSourceReaction)

  QString m_script;
  QString m_json;
};

} // namespace tomviz

#endif
