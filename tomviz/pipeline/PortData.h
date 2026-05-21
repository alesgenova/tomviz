/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePortData_h
#define tomvizPipelinePortData_h

#include "PortType.h"

#include <any>

namespace tomviz {
namespace pipeline {

class PortData
{
public:
  PortData();
  PortData(std::any data, PortType type);

  bool isValid() const;
  PortType type() const;
  const std::any& data() const;
  void clear();

  template <typename T>
  T value() const
  {
    return std::any_cast<T>(m_data);
  }

private:
  std::any m_data;
  PortType m_type = PortType::None;
};

} // namespace pipeline
} // namespace tomviz

#endif
