###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Core graph types: Node, SourceNode, TransformNode, SinkNode, Pipeline,
Link, Port, InputPort, OutputPort, PortData, NodeState. Mirrors the C++
class shapes under tomviz/pipeline/."""

from __future__ import annotations

import enum
from typing import Any


class NodeState(enum.Enum):
    New = 'New'
    Stale = 'Stale'
    Current = 'Current'


class PortData:
    """Typed payload flowing through a port. Mirrors the C++ PortData which
    holds a std::any plus a PortType. We carry an arbitrary Python payload
    (typically a tomviz.external_dataset.Dataset) and a string port type."""

    __slots__ = ('payload', 'port_type')

    def __init__(self, payload: Any, port_type: str = 'ImageData'):
        self.payload = payload
        self.port_type = port_type

    def __repr__(self):
        return f'PortData(type={self.port_type!r})'


class Port:
    def __init__(self, name: str, port_type: str):
        self.name = name
        self.port_type = port_type
        self.node: Node | None = None


class InputPort(Port):
    """Accepts a single incoming Link. accepted_types is an informational
    list — type validation is intentionally lax in the Python runtime."""

    def __init__(self, name: str, accepted_types):
        if isinstance(accepted_types, str):
            accepted_types = [accepted_types]
        self.accepted_types = list(accepted_types)
        super().__init__(name, self.accepted_types[0])
        self.link: Link | None = None

    def has_data(self) -> bool:
        return self.link is not None and self.link.from_port.has_data()

    def data(self) -> PortData | None:
        if self.link is None:
            return None
        return self.link.from_port.data()


class OutputPort(Port):
    def __init__(self, name: str, port_type: str, persistent: bool = True):
        super().__init__(name, port_type)
        self.persistent = persistent
        self._data: PortData | None = None
        self.outgoing_links: list[Link] = []

    def has_data(self) -> bool:
        return self._data is not None

    def data(self) -> PortData | None:
        return self._data

    def set_data(self, data: PortData | None):
        self._data = data


class Link:
    def __init__(self, from_port: OutputPort, to_port: InputPort):
        self.from_port = from_port
        self.to_port = to_port


class Node:
    """Base graph node. Holds typed input/output ports, a label, a state,
    and a free-form properties dict. Subclasses override execute()."""

    type_name: str = ''

    def __init__(self):
        self.id: int = -1
        self.label: str = ''
        self.state: NodeState = NodeState.New
        self.breakpoint: bool = False
        self.properties: dict[str, Any] = {}
        self.type_inference_sources: dict[str, str] = {}
        self._input_ports: list[InputPort] = []
        self._output_ports: list[OutputPort] = []
        # Progress hooks. The pipeline executor sets `progress` before
        # execute() is called; nodes (e.g. LegacyPythonTransform) forward
        # operator progress updates through it.
        self.progress = None

    # ---- ports ---------------------------------------------------------

    def add_input(self, name: str, accepted_types) -> InputPort:
        port = InputPort(name, accepted_types)
        port.node = self
        self._input_ports.append(port)
        return port

    def add_output(self, name: str, port_type: str,
                   persistent: bool = True) -> OutputPort:
        port = OutputPort(name, port_type, persistent=persistent)
        port.node = self
        self._output_ports.append(port)
        return port

    def input_ports(self) -> list[InputPort]:
        return list(self._input_ports)

    def output_ports(self) -> list[OutputPort]:
        return list(self._output_ports)

    def input_port(self, name: str) -> InputPort | None:
        for p in self._input_ports:
            if p.name == name:
                return p
        return None

    def output_port(self, name: str) -> OutputPort | None:
        for p in self._output_ports:
            if p.name == name:
                return p
        return None

    # ---- execution -----------------------------------------------------

    def execute(self) -> bool:
        """Run this node. Returns True on success, False on failure.
        Default implementation is a no-op success."""
        return True

    # ---- serialization -------------------------------------------------

    def deserialize(self, data: dict) -> bool:
        """Apply the schema-v2 JSON entry for this node. Subclasses override
        to read their type-specific fields and call super().deserialize."""
        if 'label' in data:
            self.label = data['label']
        s = data.get('state')
        if s == 'Stale':
            self.state = NodeState.Stale
        elif s == 'Current':
            self.state = NodeState.Current
        if data.get('breakpoint'):
            self.breakpoint = True
        if 'properties' in data:
            self.properties = dict(data['properties'])
        if 'typeInferenceSources' in data:
            self.type_inference_sources = dict(data['typeInferenceSources'])
        # Output ports may carry a saved declaredType — apply it if present
        # so downstream type inference matches what the C++ side saved.
        if 'outputPorts' in data:
            for name, entry in data['outputPorts'].items():
                port = self.output_port(name)
                if port is None:
                    continue
                t = entry.get('type')
                if t:
                    port.port_type = t
                if 'persistent' in entry:
                    port.persistent = bool(entry['persistent'])
        return True


class SourceNode(Node):
    """A node with only output ports. Subclasses populate output port data
    in execute() (e.g. by reading a file)."""

    type_name = 'source.generic'

    def deserialize(self, data: dict) -> bool:
        # Generic SourceNode declares its outputs only via the JSON
        # outputPorts map — create matching ports if they don't already exist
        # so Node.deserialize can apply per-port state.
        if 'outputPorts' in data:
            for name, entry in data['outputPorts'].items():
                if self.output_port(name) is None:
                    self.add_output(
                        name, entry.get('type', 'ImageData'),
                        persistent=bool(entry.get('persistent', True)))
        return super().deserialize(data)


class TransformNode(Node):
    """A node with both input and output ports. Subclasses implement
    transform(inputs) → {output_name: PortData}; execute() pulls inputs
    from connected ports, calls transform, and stores results on outputs."""

    def execute(self) -> bool:
        inputs: dict[str, PortData] = {}
        for port in self._input_ports:
            data = port.data()
            if data is None:
                # Missing input — treat as failure so downstream cascades
                # mark stale.
                return False
            inputs[port.name] = data

        result = self.transform(inputs)
        if result is None:
            return False

        for name, data in result.items():
            port = self.output_port(name)
            if port is not None:
                port.set_data(data)
        return True

    def transform(self, inputs: dict[str, PortData]) -> dict[str, PortData]:
        raise NotImplementedError(
            f'{type(self).__name__}.transform is not implemented')


class SinkNode(Node):
    """A node with only input ports. The pure-Python CLI ignores sinks at
    execution time (no rendering); they exist only so state files load
    cleanly. Subclasses are unused — a single placeholder class registered
    under each sink type string is enough."""

    def execute(self) -> bool:
        # Sinks are intentionally inert in the CLI runtime.
        return True


class Pipeline:
    """Directed graph of Node objects connected by Link objects."""

    def __init__(self):
        self.nodes: list[Node] = []
        self.links: list[Link] = []
        self._next_node_id: int = 1

    def add_node(self, node: Node):
        if node.id < 0:
            node.id = self._next_node_id
            self._next_node_id += 1
        else:
            self._next_node_id = max(self._next_node_id, node.id + 1)
        self.nodes.append(node)

    def set_node_id(self, node: Node, node_id: int):
        node.id = node_id
        self._next_node_id = max(self._next_node_id, node_id + 1)

    def node_by_id(self, node_id: int) -> Node | None:
        for n in self.nodes:
            if n.id == node_id:
                return n
        return None

    def create_link(self, from_port: OutputPort,
                    to_port: InputPort) -> Link:
        link = Link(from_port, to_port)
        from_port.outgoing_links.append(link)
        to_port.link = link
        self.links.append(link)
        return link

    # ---- topology ------------------------------------------------------

    def upstream_nodes(self, node: Node) -> list[Node]:
        nodes: list[Node] = []
        for port in node.input_ports():
            if port.link is not None and port.link.from_port.node is not None:
                nodes.append(port.link.from_port.node)
        return nodes

    def downstream_nodes(self, node: Node) -> list[Node]:
        nodes: list[Node] = []
        for port in node.output_ports():
            for link in port.outgoing_links:
                if link.to_port.node is not None:
                    nodes.append(link.to_port.node)
        return nodes

    def execution_order(self) -> list[Node]:
        """Kahn topological sort. Stable: ties broken by node id, then by
        the order nodes were added."""
        in_degree: dict[int, int] = {}
        for n in self.nodes:
            in_degree[id(n)] = 0
        for link in self.links:
            tn = link.to_port.node
            if tn is not None:
                in_degree[id(tn)] += 1

        ready = [n for n in self.nodes if in_degree[id(n)] == 0]
        ready.sort(key=lambda n: (n.id if n.id >= 0 else 1 << 30))

        order: list[Node] = []
        while ready:
            n = ready.pop(0)
            order.append(n)
            for d in self.downstream_nodes(n):
                in_degree[id(d)] -= 1
                if in_degree[id(d)] == 0:
                    ready.append(d)
            ready.sort(key=lambda n: (n.id if n.id >= 0 else 1 << 30))

        if len(order) != len(self.nodes):
            raise RuntimeError('Pipeline contains a cycle')
        return order
