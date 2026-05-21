/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSaveLoadTemplateReaction_h
#define tomvizSaveLoadTemplateReaction_h

#include <pqReaction.h>

#include <QString>

namespace tomviz {

class SaveLoadTemplateReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveLoadTemplateReaction(QAction* action, bool load = false, QString filename = QString());

  static bool saveTemplate();
  static bool saveTemplate(const QString& filename);
  /// Pop a Save dialog so the user can choose any path for the .tvsm.
  /// Used by the File > Save Template As… action.
  static bool saveTemplateAs();
  static bool loadTemplate(const QString& filename);
  /// Pop a file dialog accepting .tvsm or .tvh5 (any schema). On success
  /// the file is pushed onto the recent template files list. Used by
  /// the File > Load Template… action.
  static bool loadTemplateWithDialog();

protected:
  void onTriggered() override;

private:
  Q_DISABLE_COPY(SaveLoadTemplateReaction)
  bool m_load;
  QString m_filename;
};
} // namespace tomviz

#endif
