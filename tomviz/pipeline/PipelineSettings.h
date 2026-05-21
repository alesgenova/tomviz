/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePipelineSettings_h
#define tomvizPipelinePipelineSettings_h

#include <QObject>

namespace tomviz {
namespace pipeline {

/// Application-wide default for transform output port persistence,
/// applied when a node doesn't explicitly request a specific mode
/// (e.g. a schema-v2 operator JSON without a `persistent` field, or a
/// C++ TransformNode subclass that just calls addOutput).
enum class TransformPersistenceDefault
{
  InMemory, ///< persistent + InMemory
  OnDisk,   ///< persistent + OnDisk
  Transient ///< not persistent (released when consumers drop)
};

/// Singleton accessor for the application-wide pipeline defaults.
/// Backed by QSettings so the choice survives restarts.
class PipelineSettings : public QObject
{
  Q_OBJECT

public:
  static PipelineSettings& instance();

  TransformPersistenceDefault transformPersistenceDefault() const;
  void setTransformPersistenceDefault(TransformPersistenceDefault mode);

signals:
  /// Emitted when the default mode changes. The PipelineControls UI
  /// uses this to keep its button group in sync across sessions /
  /// other widgets that might also expose the setting.
  void transformPersistenceDefaultChanged(TransformPersistenceDefault mode);

private:
  PipelineSettings();
  TransformPersistenceDefault m_transformDefault;
};

} // namespace pipeline
} // namespace tomviz

#endif
