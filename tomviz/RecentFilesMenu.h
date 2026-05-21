/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRecentFilesMenu_h
#define tomvizRecentFilesMenu_h

#include <QObject>

class QAction;
class QMenu;

namespace tomviz {

namespace pipeline {
class SourceNode;
}

/// Adds recent file and recent state file support to Tomviz.
class RecentFilesMenu : public QObject
{
  Q_OBJECT

public:
  RecentFilesMenu(QMenu& menu, QObject* parent = nullptr);
  ~RecentFilesMenu() override;

  /// Pushes a reader on the recent files stack.
  static void pushDataReader(pipeline::SourceNode* source);
  static void pushStateFile(const QString& filename);
  static void pushTemplateFile(const QString& filename);

private slots:
  void aboutToShowMenu();
  void dataSourceTriggered(QAction* actn, QStringList fileNames);
  void stateTriggered();
  void templateTriggered();

private:
  Q_DISABLE_COPY(RecentFilesMenu)
};
} // namespace tomviz
#endif
