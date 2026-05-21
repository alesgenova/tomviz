/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePort_h
#define tomvizPipelinePort_h

#include <QObject>
#include <QString>

namespace tomviz {
namespace pipeline {

class Node;

class Port : public QObject
{
  Q_OBJECT

public:
  Port(const QString& name, QObject* parent = nullptr);
  ~Port() override = default;

  QString name() const;
  void setName(const QString& name);
  Node* node() const;

signals:
  void connectionChanged();

private:
  QString m_name;
};

} // namespace pipeline
} // namespace tomviz

#endif
