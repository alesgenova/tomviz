###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Default pipeline executor: topological sort, execute each non-sink node
in order, propagate Stale state down the graph on failure. Sinks are
unconditionally skipped."""

import logging

from tomviz.pipeline.node import (
    NodeState,
    Pipeline,
    SinkNode,
    SourceNode,
    TransformNode,
)


logger = logging.getLogger('tomviz.pipeline')


class DefaultExecutor:
    def __init__(self, pipeline: Pipeline, progress=None):
        self.pipeline = pipeline
        self.progress = progress
        self._failed = False

    def execute(self) -> bool:
        order = self.pipeline.execution_order()

        for node in order:
            if isinstance(node, SinkNode):
                # CLI never executes sinks.
                continue

            # If any upstream node ended up Stale, cascade.
            if any(u.state == NodeState.Stale
                   for u in self.pipeline.upstream_nodes(node)):
                node.state = NodeState.Stale
                continue

            if node.state == NodeState.Current:
                continue

            if self.progress is not None:
                self.progress.started(node.id)
                node.progress = self.progress

            label = node.label or type(node).__name__
            logger.info("Executing '%s' (id=%d)", label, node.id)
            try:
                ok = node.execute()
            except Exception:
                logger.exception("Node '%s' (id=%d) raised", label, node.id)
                ok = False

            if self.progress is not None:
                self.progress.finished(node.id)

            if not ok:
                node.state = NodeState.Stale
                self._failed = True
                logger.error("Node '%s' (id=%d) failed", label, node.id)
                continue

            node.state = NodeState.Current

        return not self._failed

    # ---- helpers -------------------------------------------------------

    @staticmethod
    def is_source(node) -> bool:
        return isinstance(node, SourceNode)

    @staticmethod
    def is_transform(node) -> bool:
        return isinstance(node, TransformNode)

    @staticmethod
    def is_sink(node) -> bool:
        return isinstance(node, SinkNode)
