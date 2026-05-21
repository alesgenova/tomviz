/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeEditDialog_h
#define tomvizPipelineNodeEditDialog_h

#include "DeferredLinkInfo.h"

#include <QDialog>

class QDialogButtonBox;
class QShowEvent;

namespace tomviz {
namespace pipeline {

class EditNodeWidget;
class Node;
class Pipeline;

/// A dialog for configuring node parameters with Apply/OK/Cancel.
///
/// Two modes:
///   - **Insertion mode**: The node is being added. Apply/OK completes
///     the insertion and executes the pipeline. Cancel removes the node
///     (and any input/output links described by DeferredLinkInfo).
///   - **Edit mode**: The node already exists. Apply/OK re-applies
///     parameters and executes. Cancel just closes the dialog.
///
/// Editor gating (e.g. waiting for input data) is handled inside the
/// EditNodeWidget; this dialog just gates Apply/OK on canApply() and
/// the pipeline's executing state.
class NodeEditDialog : public QDialog
{
  Q_OBJECT

public:
  /// Edit an existing node (edit mode).
  NodeEditDialog(Node* node, Pipeline* pipeline, QWidget* parent = nullptr);

  /// Insert a new node (insertion mode). For source-shaped nodes the
  /// DeferredLinkInfo can be empty.
  NodeEditDialog(Node* node, Pipeline* pipeline,
                 const DeferredLinkInfo& deferred,
                 QWidget* parent = nullptr);

  ~NodeEditDialog() override;

  Node* node() const;

signals:
  /// Emitted after Apply/OK completes an insertion.
  void insertionCompleted(Node* node);

  /// Emitted after Cancel aborts an insertion.
  void insertionCanceled();

private slots:
  void onApply();
  void onOkay();
  void onCancel();

protected:
  void showEvent(QShowEvent* event) override;

private:
  void init();
  void refreshButtonEnablement();
  void saveGeometry();
  void restoreGeometry();

  void completeInsertion();

  Node* m_node;
  Pipeline* m_pipeline;
  EditNodeWidget* m_editWidget = nullptr;
  QDialogButtonBox* m_buttonBox = nullptr;
  DeferredLinkInfo m_deferred;
  bool m_isNewInsertion;
  bool m_insertionCompleted = false;
};

} // namespace pipeline
} // namespace tomviz

#endif
