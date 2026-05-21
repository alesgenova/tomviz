/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePersistenceMode_h
#define tomvizPipelinePersistenceMode_h

#include <QString>

namespace tomviz {
namespace pipeline {

/// The medium that backs a persistent output port.
///
/// Orthogonal to `isPersistent()` itself: a port is either persistent or
/// transient, and — if persistent — picks which medium holds the payload
/// across consumer-release cycles.
enum class PersistenceMode
{
  InMemory, ///< Port keeps its strong ref forever (legacy behavior).
  OnDisk    ///< Strong ref is released when no consumer holds; the
            ///< payload is serialized to a temporary file and lazily
            ///< reloaded on the next access.
};

inline QString persistenceModeToString(PersistenceMode mode)
{
  switch (mode) {
    case PersistenceMode::InMemory:
      return QStringLiteral("memory");
    case PersistenceMode::OnDisk:
      return QStringLiteral("disk");
  }
  return QStringLiteral("memory");
}

inline PersistenceMode persistenceModeFromString(const QString& s)
{
  if (s == QLatin1String("disk")) {
    return PersistenceMode::OnDisk;
  }
  return PersistenceMode::InMemory;
}

} // namespace pipeline
} // namespace tomviz

#endif
