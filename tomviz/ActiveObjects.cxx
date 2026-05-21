/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ActiveObjects.h"
#include "Utilities.h"

#include "pipeline/Link.h"
#include "pipeline/Node.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineUtils.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqRenderView.h>
#include <pqServer.h>
#include <pqView.h>

#include <vtkNew.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMViewProxy.h>

namespace tomviz {

ActiveObjects::ActiveObjects() : QObject()
{
  connect(&pqActiveObjects::instance(), &pqActiveObjects::viewChanged, this,
          QOverload<pqView*>::of(&ActiveObjects::viewChanged));
}

ActiveObjects::~ActiveObjects() = default;

ActiveObjects& ActiveObjects::instance()
{
  static ActiveObjects theInstance;
  return theInstance;
}

void ActiveObjects::setActiveView(vtkSMViewProxy* view)
{
  pqActiveObjects::instance().setActiveView(tomviz::convert<pqView*>(view));
}

vtkSMViewProxy* ActiveObjects::activeView() const
{
  pqView* view = activePqView();
  return view ? view->getViewProxy() : nullptr;
}

pqView* ActiveObjects::activePqView() const
{
  return pqActiveObjects::instance().activeView();
}

pqRenderView* ActiveObjects::activePqRenderView() const
{
  return qobject_cast<pqRenderView*>(activePqView());
}

void ActiveObjects::viewChanged(pqView* view)
{
  emit viewChanged(view ? view->getViewProxy() : nullptr);
}

vtkSMSessionProxyManager* ActiveObjects::proxyManager() const
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  return server ? server->proxyManager() : nullptr;
}

void ActiveObjects::createRenderViewIfNeeded()
{
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(proxyManager());
  iter->SetModeToOneGroup();
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    vtkSMRenderViewProxy* renderView =
      vtkSMRenderViewProxy::SafeDownCast(iter->GetProxy());
    if (renderView) {
      return;
    }
  }

  // If we get here, there was no existing view, so create one.
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  pqServer* server = pqApplicationCore::instance()->getActiveServer();
  builder->createView("RenderView", server);
}

void ActiveObjects::setActiveViewToFirstRenderView()
{
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(proxyManager());
  iter->SetModeToOneGroup();
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    vtkSMRenderViewProxy* renderView =
      vtkSMRenderViewProxy::SafeDownCast(iter->GetProxy());
    if (renderView) {
      setActiveView(renderView);
      break;
    }
  }
}

void ActiveObjects::renderAllViews()
{
  pqApplicationCore::instance()->render();
}

pqTimeKeeper* ActiveObjects::activeTimeKeeper() const
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  return server ? server->getTimeKeeper() : nullptr;
}

void ActiveObjects::setPipeline(pipeline::Pipeline* p)
{
  Q_ASSERT(!m_pipeline && "Pipeline should only be set once");
  m_pipeline = p;
  connect(p, &pipeline::Pipeline::nodeRemoved, this,
          [this](pipeline::Node* node) {
            if (m_activeNode == node) {
              setActiveNode(nullptr);
            }
            if (m_activePort && m_activePort->node() == node) {
              setActivePort(nullptr);
            }
            if (m_activeTipOutputPort &&
                m_activeTipOutputPort->node() == node) {
              setActiveTipOutputPort(
                pipeline::findTipOutputPort(m_pipeline, nullptr));
            }
          });
  connect(p, &pipeline::Pipeline::linkRemoved, this,
          [this](pipeline::Link* link) {
            if (m_activeLink == link) {
              setActiveLink(nullptr);
            }
          });
  emit activePipelineChanged(p);
}

pipeline::Pipeline* ActiveObjects::pipeline() const
{
  return m_pipeline;
}

void ActiveObjects::setActiveNode(pipeline::Node* node)
{
  // Only one of node / port / link can be active at a time. Clear the
  // others before emitting our own change so listeners (e.g. the
  // properties panel) settle on the new selection rather than briefly
  // showing it and then being wiped by a trailing port/link clear.
  if (node) {
    setActivePort(nullptr);
    setActiveLink(nullptr);
  }

  auto* prevNode = m_activeNode;
  if (m_activeNode != node) {
    m_activeNode = node;
    emit activeNodeChanged(node);
  }
  if (node) {
    auto outputs = node->outputPorts();
    if (outputs.isEmpty()) {
      setActiveTipOutputPort(
        pipeline::findTipOutputPort(m_pipeline, node));
    } else {
      setActiveTipOutputPort(outputs.first());
    }
  } else if (prevNode) {
    setActiveTipOutputPort(
      pipeline::findTipOutputPort(m_pipeline, prevNode));
  }
}

pipeline::Node* ActiveObjects::activeNode() const
{
  return m_activeNode;
}

void ActiveObjects::setActivePort(pipeline::OutputPort* port)
{
  if (port) {
    setActiveNode(nullptr);
    setActiveLink(nullptr);
  }
  if (m_activePort != port) {
    m_activePort = port;
    emit activePortChanged(port);
  }
  if (port) {
    setActiveTipOutputPort(port);
  }
}

pipeline::OutputPort* ActiveObjects::activePort() const
{
  return m_activePort;
}

void ActiveObjects::setActiveLink(pipeline::Link* link)
{
  if (link) {
    setActiveNode(nullptr);
    setActivePort(nullptr);
  }
  if (m_activeLink != link) {
    m_activeLink = link;
    emit activeLinkChanged(link);
  }
  if (link) {
    setActiveTipOutputPort(link->from());
  }
}

void ActiveObjects::clearActiveSelection()
{
  setActiveNode(nullptr);
  setActivePort(nullptr);
  setActiveLink(nullptr);
}

pipeline::Link* ActiveObjects::activeLink() const
{
  return m_activeLink;
}

pipeline::OutputPort* ActiveObjects::activeTipOutputPort() const
{
  return m_activeTipOutputPort;
}

void ActiveObjects::setActiveTipOutputPort(pipeline::OutputPort* port)
{
  if (m_activeTipOutputPort != port) {
    QObject::disconnect(m_tipEffectiveTypeConn);
    m_activeTipOutputPort = port;
    if (port) {
      m_tipEffectiveTypeConn = connect(
        port, &pipeline::OutputPort::effectiveTypeChanged,
        this, [this](pipeline::PortType) {
          emit activeTipOutputPortChanged(m_activeTipOutputPort);
        });
    }
    emit activeTipOutputPortChanged(port);
  }
}

} // end of namespace tomviz
