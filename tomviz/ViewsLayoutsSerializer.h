/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizViewsLayoutsSerializer_h
#define tomvizViewsLayoutsSerializer_h

#include <QJsonObject>

class vtkSMProxy;
class vtkSMSessionProxyManager;

namespace tomviz {

/// Serialization helper for the top-level ParaView views / layouts / palette
/// sections of a Tomviz state file. The legacy saver (ModuleManager) and the
/// new saver (PipelineStateIO) share this code so both formats emit the same
/// byte-for-byte views[]/layouts[]/paletteColor entries; the legacy loader
/// (LegacyStateLoader) is the authoritative read path for both.
class ViewsLayoutsSerializer
{
public:
  /// Write `views`, `layouts`, and `paletteColor` into @a doc.
  /// @a pxm may be null in headless / test contexts, in which case this
  /// is a no-op. @a activeView is used to mark the corresponding view
  /// entry as active; may be null.
  static void save(QJsonObject& doc, vtkSMSessionProxyManager* pxm,
                   vtkSMProxy* activeView = nullptr);

  /// Convenience overload that pulls @a pxm and @a activeView from
  /// ActiveObjects. Safe only in contexts where pqApplicationCore has
  /// been initialized (i.e. the running application, not unit tests).
  static void saveActive(QJsonObject& doc);
};

} // namespace tomviz

#endif
