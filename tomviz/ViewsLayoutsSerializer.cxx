/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ViewsLayoutsSerializer.h"

#include "ActiveObjects.h"
#include "Utilities.h"

#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>

#include <vtk_pugixml.h>

#include <QJsonArray>

namespace tomviz {

namespace {

QJsonArray jsonArrayFromXml(pugi::xml_node node)
{
  QJsonArray array;
  for (auto element = node.child("Element"); element;
       element = element.next_sibling("Element")) {
    array.append(element.attribute("value").as_int(-1));
  }
  return array;
}

QJsonArray jsonArrayFromXmlDouble(pugi::xml_node node)
{
  QJsonArray array;
  for (auto element = node.child("Element"); element;
       element = element.next_sibling("Element")) {
    array.append(element.attribute("value").as_double(-1));
  }
  return array;
}

} // namespace

void ViewsLayoutsSerializer::saveActive(QJsonObject& doc)
{
  save(doc, ActiveObjects::instance().proxyManager(),
       ActiveObjects::instance().activeView());
}

void ViewsLayoutsSerializer::save(QJsonObject& doc,
                                  vtkSMSessionProxyManager* pxm,
                                  vtkSMProxy* activeView)
{
  if (!pxm) {
    return;
  }

  // Palette color
  if (auto* paletteProxy = pxm->GetProxy("settings", "ColorPalette")) {
    double paletteColor[3];
    vtkSMPropertyHelper(paletteProxy->GetProperty("BackgroundColor"))
      .Get(paletteColor, 3);
    doc["paletteColor"] =
      QJsonArray({ paletteColor[0], paletteColor[1], paletteColor[2] });
  }

  // Serialize layouts
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(pxm);
  iter->SetModeToOneGroup();
  QJsonArray jLayouts;
  for (iter->Begin("layouts"); !iter->IsAtEnd(); iter->Next()) {
    if (vtkSMProxy* layout = iter->GetProxy()) {
      QJsonObject jLayout;
      jLayout["id"] = static_cast<int>(layout->GetGlobalID());
      jLayout["xmlGroup"] = layout->GetXMLGroup();
      jLayout["xmlName"] = layout->GetXMLName();

      pugi::xml_document document;
      auto proxyNode = document.append_child("ParaViewXML");
      tomviz::serialize(layout, proxyNode);
      auto layoutProxy = document.child("ParaViewXML").child("Proxy");
      jLayout["servers"] = layoutProxy.attribute("servers").as_int(0);
      QJsonArray layoutArray;
      for (auto node = layoutProxy.child("Layout"); node;
           node = node.next_sibling("Layout")) {
        QJsonArray itemArray;
        for (auto itemNode = node.child("Item"); itemNode;
             itemNode = itemNode.next_sibling("Item")) {
          QJsonObject itemObj;
          itemObj["direction"] = itemNode.attribute("direction").as_int(0);
          itemObj["fraction"] = itemNode.attribute("fraction").as_double(0);
          itemObj["viewId"] = itemNode.attribute("view").as_int(0);
          itemArray.append(itemObj);
        }
        layoutArray.append(itemArray);
      }
      jLayout["items"] = layoutArray;
      jLayouts.append(jLayout);
    }
  }
  if (!jLayouts.isEmpty()) {
    doc["layouts"] = jLayouts;
  }

  // Serialize views
  QJsonArray jViews;
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    if (vtkSMProxy* view = iter->GetProxy()) {
      QJsonObject jView;
      jView["id"] = static_cast<int>(view->GetGlobalID());
      jView["xmlGroup"] = view->GetXMLGroup();
      jView["xmlName"] = view->GetXMLName();
      if (activeView && view == activeView) {
        jView["active"] = true;
      }

      if (view->GetProperty("UseColorPaletteForBackground")) {
        jView["useColorPaletteForBackground"] =
          vtkSMPropertyHelper(view, "UseColorPaletteForBackground").GetAsInt();
      }

      pugi::xml_document document;
      pugi::xml_node proxyNode = document.append_child("ParaViewXML");
      tomviz::serialize(view, proxyNode);

      QJsonObject camera;

      auto viewProxy = document.child("ParaViewXML").child("Proxy");
      jView["servers"] = viewProxy.attribute("servers").as_int(0);
      QJsonArray backgroundColor;
      for (pugi::xml_node node = viewProxy.child("Property"); node;
           node = node.next_sibling("Property")) {
        std::string name = node.attribute("name").as_string("");
        if (name == "ViewSize") {
          jView["viewSize"] = jsonArrayFromXml(node);
        } else if (name == "CameraFocalPoint") {
          camera["focalPoint"] = jsonArrayFromXmlDouble(node);
        } else if (name == "CameraPosition") {
          camera["position"] = jsonArrayFromXmlDouble(node);
        } else if (name == "CameraViewUp") {
          camera["viewUp"] = jsonArrayFromXmlDouble(node);
        } else if (name == "CameraViewAngle") {
          camera["viewAngle"] = jsonArrayFromXmlDouble(node)[0];
        } else if (name == "EyeAngle") {
          camera["eyeAngle"] = jsonArrayFromXmlDouble(node)[0];
        } else if (name == "CenterOfRotation") {
          jView["centerOfRotation"] = jsonArrayFromXmlDouble(node);
        } else if (name == "Background") {
          backgroundColor.append(jsonArrayFromXmlDouble(node));
        } else if (name == "Background2") {
          vtkSMPropertyHelper helper(view, "BackgroundColorMode");
          if (helper.GetAsString() == std::string("Gradient")) {
            backgroundColor.append(jsonArrayFromXmlDouble(node));
          }
        } else if (name == "CameraParallelScale") {
          camera["parallelScale"] = jsonArrayFromXmlDouble(node)[0];
        } else if (name == "CameraParallelProjection") {
          vtkSMPropertyHelper helper(view, "CameraParallelProjection");
          jView["isOrthographic"] = helper.GetAsInt() != 0;
        } else if (name == "InteractionMode") {
          vtkSMPropertyHelper helper(view, "InteractionMode");
          QString mode = "3D";
          if (helper.GetAsInt() == 1) {
            mode = "2D";
          } else if (helper.GetAsInt() == 2) {
            mode = "selection";
          }
          jView["interactionMode"] = mode;
        } else if (name == "CenterAxesVisibility") {
          vtkSMPropertyHelper helper(view, "CenterAxesVisibility");
          jView["centerAxesVisible"] = helper.GetAsInt() == 1;
        } else if (name == "OrientationAxesVisibility") {
          vtkSMPropertyHelper helper(view, "OrientationAxesVisibility");
          jView["orientationAxesVisible"] = helper.GetAsInt() == 1;
        }
      }
      if (view->GetProperty("AxesGrid")) {
        vtkSMPropertyHelper helper(view, "AxesGrid");
        vtkSMProxy* axesGridProxy = helper.GetAsProxy();
        if (axesGridProxy) {
          vtkSMPropertyHelper visibilityHelper(axesGridProxy, "Visibility");
          jView["axesGridVisibility"] = visibilityHelper.GetAsInt() != 0;
        }
      }
      jView["camera"] = camera;
      jView["backgroundColor"] = backgroundColor;

      jViews.append(jView);
    }
  }
  if (!jViews.isEmpty()) {
    doc["views"] = jViews;
  }
}

} // namespace tomviz
