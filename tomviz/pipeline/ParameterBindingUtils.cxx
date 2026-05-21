/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ParameterBindingUtils.h"

#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "OutputPort.h"
#include "SinkGroupNode.h"
#include "sinks/SliceSink.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QWidget>

namespace tomviz {
namespace pipeline {

namespace {

/// Walk @a port's outgoing links for a sink of type @a SinkT
/// satisfying @a ok, treating a SinkGroupNode as transparent: when a
/// link lands on a group's input, the search descends through the
/// matching passthrough output port and continues. Recurses to handle
/// nested groups.
template <typename SinkT, typename Pred>
SinkT* findSinkOnOutputPort(OutputPort* port, Pred ok)
{
  if (!port) {
    return nullptr;
  }
  for (auto* link : port->links()) {
    auto* dst = link->to();
    if (!dst) {
      continue;
    }
    auto* dstNode = dst->node();
    if (auto* sink = qobject_cast<SinkT*>(dstNode)) {
      if (ok(sink)) {
        return sink;
      }
    }
    if (auto* group = qobject_cast<SinkGroupNode*>(dstNode)) {
      if (auto* deep =
            findSinkOnOutputPort<SinkT>(group->outputPort(dst->name()), ok)) {
        return deep;
      }
    }
  }
  return nullptr;
}

/// Walk @a node's port topology looking for a sink of type @a SinkT
/// satisfying @a ok. Sibling sinks on each upstream output port are
/// preferred (1st-preference: a sink already attached to the same
/// branch the input feeds from); otherwise sinks downstream of any of
/// @a node's own output ports. Sinks behind a SinkGroupNode are
/// reachable through the group's passthrough.
template <typename SinkT, typename Pred>
SinkT* findBindableSink(const Node* node, Pred ok)
{
  for (auto* in : node->inputPorts()) {
    auto* link = in->link();
    if (!link) {
      continue;
    }
    if (auto* sink = findSinkOnOutputPort<SinkT>(link->from(), ok)) {
      return sink;
    }
  }
  for (auto* out : node->outputPorts()) {
    if (auto* sink = findSinkOnOutputPort<SinkT>(out, ok)) {
      return sink;
    }
  }
  return nullptr;
}

/// Live link a QSpinBox to a SliceSink::slice property. Cycle is
/// broken by the value-equality guard; QSignalBlocker is belt-and-
/// suspenders for the seed and any signal interleavings.
void wireSliceSinkBinding(QWidget* widget, QSpinBox* spin, SliceSink* sink)
{
  {
    QSignalBlocker b(spin);
    spin->setValue(sink->slice());
  }

  QPointer<SliceSink> sinkPtr(sink);
  QObject::connect(sink, &SliceSink::sliceChanged, widget,
                   [spin, sinkPtr](int value) {
                     if (!sinkPtr || spin->value() == value) {
                       return;
                     }
                     QSignalBlocker b(spin);
                     spin->setValue(value);
                   });
  QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), widget,
                   [sinkPtr](int value) {
                     if (!sinkPtr || sinkPtr->slice() == value) {
                       return;
                     }
                     sinkPtr->setSlice(value);
                   });
}

} // namespace

QMap<QString, ParameterBinding> parseParameterBindings(
  const QJsonObject& description)
{
  QMap<QString, ParameterBinding> bindings;
  for (const auto& v : description.value(QStringLiteral("parameters")).toArray()) {
    QJsonObject param = v.toObject();
    QString name = param.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) {
      continue;
    }
    QJsonValue bindVal = param.value(QStringLiteral("bindToSink"));
    if (!bindVal.isObject()) {
      continue;
    }
    QJsonObject bindObj = bindVal.toObject();
    ParameterBinding b;
    b.sinkType = bindObj.value(QStringLiteral("type")).toString();
    b.sinkProperty = bindObj.value(QStringLiteral("property")).toString();
    if (b.sinkType.isEmpty() || b.sinkProperty.isEmpty()) {
      continue;
    }
    bindings[name] = b;
  }
  return bindings;
}

void wireParameterBindings(Node* node, QWidget* widget,
                           const QMap<QString, ParameterBinding>& bindings)
{
  if (!node || !widget || bindings.isEmpty()) {
    return;
  }

  for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
    const QString& paramName = it.key();
    const ParameterBinding& binding = it.value();

    if (binding.sinkType == QStringLiteral("SliceSink") &&
        binding.sinkProperty == QStringLiteral("slice")) {
      auto* sink = findBindableSink<SliceSink>(
        node, [](SliceSink* s) { return s->isOrtho(); });
      if (!sink) {
        continue;
      }
      // ParameterInterfaceBuilder names each numeric control after its
      // parameter, so a recursive findChild reaches the spinbox no
      // matter where it lives in the tab/sub-widget tree.
      auto* spin = widget->findChild<QSpinBox*>(paramName);
      if (!spin) {
        continue;
      }
      wireSliceSinkBinding(widget, spin, sink);
    }
    // Future sink types: add another dispatch arm here.
  }
}

} // namespace pipeline
} // namespace tomviz
