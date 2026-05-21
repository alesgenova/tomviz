/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CustomNodeWidgetRegistry.h"

#include <QHash>

namespace tomviz {
namespace pipeline {

namespace {

QHash<QString, CustomWidgetInfo>& registry()
{
  static QHash<QString, CustomWidgetInfo> r;
  return r;
}

} // namespace

void registerCustomNodeWidget(const QString& id,
                              const CustomWidgetInfo& info)
{
  registry().insert(id, info);
}

const CustomWidgetInfo* findCustomNodeWidget(const QString& id)
{
  auto& r = registry();
  auto it = r.constFind(id);
  if (it == r.constEnd()) {
    return nullptr;
  }
  return &it.value();
}

} // namespace pipeline
} // namespace tomviz
