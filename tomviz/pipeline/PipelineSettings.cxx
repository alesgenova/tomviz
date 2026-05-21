/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineSettings.h"

#include <QSettings>

namespace tomviz {
namespace pipeline {

namespace {

constexpr auto kKey = "pipeline/transformPersistenceDefault";
constexpr auto kInMemory = "InMemory";
constexpr auto kOnDisk = "OnDisk";
constexpr auto kTransient = "Transient";

TransformPersistenceDefault fromString(const QString& s)
{
  if (s == QLatin1String(kInMemory)) {
    return TransformPersistenceDefault::InMemory;
  }
  if (s == QLatin1String(kTransient)) {
    return TransformPersistenceDefault::Transient;
  }
  return TransformPersistenceDefault::OnDisk;
}

QString toString(TransformPersistenceDefault mode)
{
  switch (mode) {
    case TransformPersistenceDefault::InMemory:
      return QString::fromLatin1(kInMemory);
    case TransformPersistenceDefault::OnDisk:
      return QString::fromLatin1(kOnDisk);
    case TransformPersistenceDefault::Transient:
      return QString::fromLatin1(kTransient);
  }
  return QString::fromLatin1(kOnDisk);
}

} // namespace

PipelineSettings& PipelineSettings::instance()
{
  static PipelineSettings s;
  return s;
}

PipelineSettings::PipelineSettings()
{
  QSettings s;
  m_transformDefault =
    fromString(s.value(kKey, QString::fromLatin1(kOnDisk)).toString());
}

TransformPersistenceDefault PipelineSettings::transformPersistenceDefault() const
{
  return m_transformDefault;
}

void PipelineSettings::setTransformPersistenceDefault(
  TransformPersistenceDefault mode)
{
  if (m_transformDefault == mode) {
    return;
  }
  m_transformDefault = mode;
  QSettings s;
  s.setValue(kKey, toString(mode));
  emit transformPersistenceDefaultChanged(mode);
}

} // namespace pipeline
} // namespace tomviz
