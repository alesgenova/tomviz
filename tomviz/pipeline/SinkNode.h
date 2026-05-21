/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSinkNode_h
#define tomvizPipelineSinkNode_h

#include "Node.h"
#include "PortData.h"

#include <QMap>
#include <QString>

#include <memory>

namespace tomviz {
namespace pipeline {

class SinkNode : public Node
{
  Q_OBJECT

public:
  SinkNode(QObject* parent = nullptr);
  ~SinkNode() override = default;

  InputPort* addInput(const QString& name, PortTypes acceptedTypes);

  bool execute() override;

protected:
  virtual bool consume(const QMap<QString, PortData>& inputs) = 0;

  /// Hook fired before consume() in both execute() and
  /// onIntermediateData() paths. Default no-op.
  virtual void prepareConsume(const QMap<QString, PortData>& /*inputs*/)
  {}

  /// Hook fired after consume() in both execute() and
  /// onIntermediateData() paths. Default no-op.
  virtual void postConsume(bool /*success*/) {}

  /// Hook fired when an input port loses its incoming link.
  /// Default no-op.
  virtual void onInputDisconnected(InputPort* /*port*/) {}

private slots:
  void onIntermediateData();

private:
  void connectUpstreamIntermediate(InputPort* port);

  /// Retained handles to the input payloads seen during the last
  /// successful consume(). Two effects: the shared_ptr keeps the
  /// upstream port's heap PortData alive (so the producer port's
  /// hasData() stays honest for UI inspection — a value-copy of
  /// PortData alone would let the heap object die and m_weak expire,
  /// even though the heavy payload inside the std::any survives), and
  /// it keeps the heavy payload pinned so the visualization works
  /// after the executor's in-flight refs have dropped. Cleared on
  /// disconnect.
  QMap<QString, std::shared_ptr<PortData>> m_retainedInputs;
};

} // namespace pipeline
} // namespace tomviz

#endif
