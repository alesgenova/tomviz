/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PortData.h"

namespace tomviz {
namespace pipeline {

PortData::PortData() = default;

PortData::PortData(std::any data, PortType type)
  : m_data(std::move(data)), m_type(type)
{}

bool PortData::isValid() const
{
  return m_data.has_value() && m_type != PortType::None;
}

PortType PortData::type() const
{
  return m_type;
}

const std::any& PortData::data() const
{
  return m_data;
}

void PortData::clear()
{
  m_data.reset();
  m_type = PortType::None;
}

} // namespace pipeline
} // namespace tomviz
