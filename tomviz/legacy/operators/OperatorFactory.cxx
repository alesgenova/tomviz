/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorFactory.h"

#include "OperatorPython.h"
#include <QDebug>
#include <QThread>

namespace tomviz {

OperatorFactory::OperatorFactory() = default;

OperatorFactory::~OperatorFactory() = default;

OperatorFactory& OperatorFactory::instance()
{
  static OperatorFactory theInstance;
  return theInstance;
}

QList<QString> OperatorFactory::operatorTypes()
{
  QList<QString> reply;
  reply << "Python";
  return reply;
}

Operator* OperatorFactory::createConvertToVolumeOperator(
  DataSource::DataSourceType)
{
  return nullptr;
}

Operator* OperatorFactory::createOperator(const QString& type, DataSource* ds)
{
  Operator* op = nullptr;
  if (type == "Python") {
    op = new OperatorPython(ds);
  }
  return op;
}

const char* OperatorFactory::operatorType(const Operator* op)
{
  if (qobject_cast<const OperatorPython*>(op)) {
    return "Python";
  }
  return nullptr;
}

void OperatorFactory::registerPythonOperator(
  const QString& label, const QString& source, bool requiresTiltSeries,
  bool requiresVolume, bool requiresFib, const QString& json)
{
  PythonOperatorInfo info;
  info.label = label;
  info.source = source;
  info.requiresTiltSeries = requiresTiltSeries;
  info.requiresVolume = requiresVolume;
  info.requiresFib = requiresFib;
  info.json = json;

  m_pythonOperators.append(info);
}

const QList<PythonOperatorInfo>& OperatorFactory::registeredPythonOperators()
{
  return m_pythonOperators;
}

} // namespace tomviz
