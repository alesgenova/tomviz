/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorSearchDialog_h
#define tomvizOperatorSearchDialog_h

#include <QDialog>
#include <QList>
#include <QString>

class QAction;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QPushButton;
class QShowEvent;

namespace tomviz {

/// A ParaView-style quick-launch dialog for searching and activating operators.
///
/// Layout (top to bottom):
///   - Search text field
///   - Button showing selected operator name (click to activate)
///   - Available operators list
///   - Description label for selected operator
///   - "Unavailable:" label + disabled operators list
///   - Instructions label
class OperatorSearchDialog : public QDialog
{
  Q_OBJECT

public:
  explicit OperatorSearchDialog(QWidget* parent = nullptr);

  /// Register an action with a category and optional description.
  void addOperatorAction(QAction* action, const QString& category,
                         const QString& description = QString());

  /// Recursively collect all actions from a menu hierarchy.
  void collectActionsFromMenu(QMenu* menu, const QString& prefix);

  void showEvent(QShowEvent* event) override;

private slots:
  void filterOperators(const QString& text);
  void availableItemChanged(QListWidgetItem* current, QListWidgetItem* prev);
  void unavailableItemChanged(QListWidgetItem* current, QListWidgetItem* prev);
  void activateCurrentProxy();

private:
  void keyPressEvent(QKeyEvent* event) override;
  void rebuildLists();
  void updateSelection(QListWidgetItem* item);
  QListWidget* currentList();

  struct OperatorEntry
  {
    QAction* action;
    QString name;
    QString category;
    QString description;
  };

  QLineEdit* m_searchBox;
  QPushButton* m_createButton;
  QListWidget* m_availableList;
  QLabel* m_descriptionLabel;
  QLabel* m_unavailableLabel;
  QListWidget* m_unavailableList;
  QLabel* m_instructionsLabel;
  QList<OperatorEntry> m_operators;
};

} // namespace tomviz

#endif
