/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonNodeUtils.h"

#include "OutputPort.h"
#include "PortType.h"
#include "data/VolumeData.h"

#pragma push_macro("slots")
#undef slots
#include <pybind11/eval.h>
#pragma pop_macro("slots")

#include <vtkImageData.h>
#include <vtkMolecule.h>
#include <vtkPythonUtil.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>

#include <QJsonObject>
#include <QVariantList>

namespace py = pybind11;

namespace tomviz {
namespace pipeline {
namespace PythonNodeUtils {

QVariant coerceJsonByDeclaredType(const QJsonValue& value,
                                  const QString& type)
{
  if (value.isArray()) {
    QVariantList list;
    for (const auto& item : value.toArray()) {
      if (type == "int" || type == "integer") {
        list.append(item.toInt());
      } else if (type == "double") {
        list.append(item.toDouble());
      } else {
        list.append(item.toVariant());
      }
    }
    return list;
  }
  if (type == "int" || type == "integer") {
    return QVariant(value.toInt());
  }
  if (type == "enumeration") {
    // Enumeration option values can be either int or string; preserve
    // the JSON-native type so saved values round-trip losslessly.
    if (value.isString()) {
      return QVariant(value.toString());
    }
    return QVariant(value.toInt());
  }
  if (type == "double") {
    return QVariant(value.toDouble());
  }
  if (type == "bool" || type == "boolean") {
    return QVariant(value.toBool());
  }
  if (type == "string" || type == "file" || type == "save_file" ||
      type == "directory") {
    return QVariant(value.toString());
  }
  return QVariant();
}

QVariant resolveEnumValue(const QJsonValue& value, const QJsonArray& options)
{
  if (value.isString()) {
    return value.toVariant();
  }
  if (value.isDouble() && !options.isEmpty()) {
    int idx = value.toInt();
    if (idx >= 0 && idx < options.size()) {
      QJsonObject opt = options.at(idx).toObject();
      if (!opt.isEmpty()) {
        return opt.constBegin().value().toVariant();
      }
    }
  }
  return QVariant();
}

py::object qvariantToPython(const QVariant& value)
{
  switch (value.typeId()) {
    case QMetaType::Double:
      return py::float_(value.toDouble());
    case QMetaType::Int:
      return py::int_(value.toInt());
    case QMetaType::Bool:
      return py::bool_(value.toBool());
    case QMetaType::QString:
      return py::str(value.toString().toStdString());
    case QMetaType::QVariantList: {
      py::list pyList;
      for (const auto& item : value.toList()) {
        pyList.append(qvariantToPython(item));
      }
      return pyList;
    }
    default:
      // Defensive fallback: cast to float. Operator parameters are
      // always one of the listed types in practice (loaded from JSON +
      // coerced via the operator description's declared type), so this
      // arm is essentially unreachable; keeping the legacy behavior
      // here for parity.
      return py::float_(value.toDouble());
  }
}

py::object loadScriptAsModule(const QString& name, const QString& script)
{
  py::module_ types = py::module_::import("types");
  py::object moduleType = types.attr("ModuleType");
  py::object module = moduleType(py::str(name.toStdString()));
  py::exec(py::str(script.toStdString()), module.attr("__dict__"));
  return module;
}

py::object findNodeClass(py::object module, py::object baseClass)
{
  py::module_ inspect = py::module_::import("inspect");
  py::object isclass = inspect.attr("isclass");
  py::object getmembers = inspect.attr("getmembers");

  py::object found = py::none();
  py::object members = getmembers(module, isclass);
  for (auto item : members) {
    py::tuple pair = py::reinterpret_borrow<py::tuple>(item);
    py::object cls = pair[1];
    // Skip the base class itself.
    if (cls.is(baseClass)) {
      continue;
    }
    if (!PyObject_IsSubclass(cls.ptr(), baseClass.ptr())) {
      // PyObject_IsSubclass returns -1 on error; py::error_already_set
      // would have been thrown by pybind11's exception translation, so
      // any non-truthy non-throwing return means "not a subclass".
      continue;
    }
    if (!found.is_none()) {
      throw py::value_error(
        "Multiple node classes defined in module — only one node class "
        "can be defined per script.");
    }
    found = cls;
  }
  return found;
}

PortData pythonValueToPortData(py::object pyValue, OutputPort* port)
{
  if (!port || pyValue.is_none()) {
    return PortData();
  }
  py::object dataObj = pyValue;
  if (py::hasattr(pyValue, "_data_object")) {
    dataObj = pyValue.attr("_data_object");
  }
  void* raw =
    vtkPythonUtil::GetPointerFromObject(dataObj.ptr(), "vtkObjectBase");
  if (!raw) {
    return PortData();
  }
  PortType type = port->type();
  if (isVolumeType(type)) {
    if (auto* image =
          vtkImageData::SafeDownCast(static_cast<vtkObjectBase*>(raw))) {
      auto vol = std::make_shared<VolumeData>(image);
      return PortData(std::any(vol), type);
    }
  } else if (type == PortType::Table) {
    if (auto* table =
          vtkTable::SafeDownCast(static_cast<vtkObjectBase*>(raw))) {
      vtkSmartPointer<vtkTable> sp(table);
      return PortData(std::any(sp), type);
    }
  } else if (type == PortType::Molecule) {
    if (auto* mol =
          vtkMolecule::SafeDownCast(static_cast<vtkObjectBase*>(raw))) {
      vtkSmartPointer<vtkMolecule> sp(mol);
      return PortData(std::any(sp), type);
    }
  }
  return PortData();
}

} // namespace PythonNodeUtils
} // namespace pipeline
} // namespace tomviz
