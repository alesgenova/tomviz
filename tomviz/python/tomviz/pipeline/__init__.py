###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Pure-Python implementation of the tomviz graph pipeline runtime, mirroring
the C++ tomviz::pipeline namespace closely enough that the same schema-v2
state files load and execute identically.

Sinks (visualization endpoints) are recognized as a node type but never
executed; the CLI ignores them. The leaves of the graph after sinks are
stripped become the CLI's output set."""

from tomviz.pipeline.factory import NodeFactory, register_builtins
from tomviz.pipeline.runner import run
from tomviz.pipeline.node import (
    InputPort,
    Link,
    Node,
    NodeState,
    OutputPort,
    Pipeline,
    PortData,
    SinkNode,
    SourceNode,
    TransformNode,
)
from tomviz.pipeline.executor import DefaultExecutor
from tomviz.pipeline.state_io import load_state

__all__ = [
    'DefaultExecutor',
    'InputPort',
    'Link',
    'Node',
    'NodeFactory',
    'NodeState',
    'OutputPort',
    'Pipeline',
    'PortData',
    'SinkNode',
    'SourceNode',
    'TransformNode',
    'load_state',
    'register_builtins',
    'run',
]
