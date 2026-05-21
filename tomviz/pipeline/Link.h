/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineLink_h
#define tomvizPipelineLink_h

#include <QObject>

namespace tomviz {
namespace pipeline {

class InputPort;
class OutputPort;

class Link : public QObject
{
  Q_OBJECT

public:
  Link(OutputPort* from, InputPort* to, QObject* parent = nullptr);
  ~Link() override;

  OutputPort* from() const;
  InputPort* to() const;

  /// True if both endpoints are non-null AND the output's effective type is
  /// compatible with the input's accepted types.  Invalid links are kept in
  /// the graph but block execution; they become valid again when upstream
  /// type changes restore compatibility.
  bool isValid() const;

  /// True if both endpoints are non-null (ignoring type compatibility).
  bool isConnected() const;

  /// Re-evaluate validity based on current effective types.
  /// Emits validityChanged() if the result changed.
  void recheck();

signals:
  void aboutToBeRemoved();
  void validityChanged(bool valid);

private:
  OutputPort* m_from;
  InputPort* m_to;
  bool m_valid = true;
};

} // namespace pipeline
} // namespace tomviz

#endif
