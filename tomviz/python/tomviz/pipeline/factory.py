###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Type-string → Node constructor registry. Mirrors the C++ NodeFactory in
naming so schema-v2 state files round-trip without renaming. Sinks are all
mapped to a single inert SinkNode subclass — the CLI never executes them."""

from typing import Callable

from tomviz.pipeline.node import Node, SinkNode, SourceNode


class NodeFactory:
    _creators: dict[str, Callable[[], Node]] = {}

    @classmethod
    def register(cls, type_name: str, ctor: Callable[[], Node]):
        cls._creators[type_name] = ctor

    @classmethod
    def create(cls, type_name: str) -> Node | None:
        ctor = cls._creators.get(type_name)
        if ctor is None:
            return None
        return ctor()

    @classmethod
    def known_types(cls) -> list[str]:
        return list(cls._creators.keys())


# All sink type strings collapse to the same inert class. We don't need
# per-sink behavior in the CLI; deserialize() on the base SinkNode is
# enough to load and ignore them.
_SINK_TYPES = (
    'sink.clip',
    'sink.contour',
    'sink.molecule',
    'sink.outline',
    'sink.plot',
    'sink.ruler',
    'sink.scaleCube',
    'sink.segment',
    'sink.slice',
    'sink.threshold',
    'sink.volume',
    'sink.volumeStats',
)


_registered = False


def register_builtins():
    """Register all node types known to the CLI. Idempotent."""
    global _registered
    if _registered:
        return
    _registered = True

    # Source types
    NodeFactory.register('source.generic', SourceNode)
    from tomviz.pipeline.sources.reader import ReaderSourceNode
    NodeFactory.register('source.reader', ReaderSourceNode)
    from tomviz.pipeline.sources.python_source import PythonSource
    NodeFactory.register('source.python', PythonSource)

    # Transform types
    from tomviz.pipeline.transforms.legacy_python import LegacyPythonTransform
    from tomviz.pipeline.transforms.python_transform import PythonTransform
    from tomviz.pipeline.transforms.convert_to_volume import (
        ConvertToVolumeTransform,
    )
    from tomviz.pipeline.transforms.set_tilt_angles import (
        SetTiltAnglesTransform,
    )
    from tomviz.pipeline.transforms.convert_to_float import (
        ConvertToFloatTransform,
    )
    from tomviz.pipeline.transforms.transpose import TransposeDataTransform
    from tomviz.pipeline.transforms.crop import CropTransform
    from tomviz.pipeline.transforms.cylindrical_crop import (
        CylindricalCropTransform,
    )
    from tomviz.pipeline.transforms.threshold import ThresholdTransform

    NodeFactory.register('transform.legacyPython', LegacyPythonTransform)
    NodeFactory.register('transform.python', PythonTransform)
    NodeFactory.register('transform.convertToVolume', ConvertToVolumeTransform)
    NodeFactory.register('transform.setTiltAngles', SetTiltAnglesTransform)
    NodeFactory.register('transform.convertToFloat', ConvertToFloatTransform)
    NodeFactory.register('transform.transposeData', TransposeDataTransform)
    NodeFactory.register('transform.crop', CropTransform)
    NodeFactory.register('transform.cylindricalCrop', CylindricalCropTransform)
    NodeFactory.register('transform.threshold', ThresholdTransform)

    # Sinks: all collapse to the inert SinkNode placeholder. Each defines
    # its own input port set lazily when deserialize() inspects the JSON.
    for t in _SINK_TYPES:
        NodeFactory.register(t, _make_inert_sink)
    NodeFactory.register('sinkGroup', _make_inert_sink)


class _InertSink(SinkNode):
    """SinkNode that adopts whatever input AND output ports the saved
    state declares. The output-port branch is for `sinkGroup` nodes,
    which on the C++ side are passthrough containers; for the CLI we
    never execute them but downstream links still need their output
    ports to exist so loading resolves correctly. Real sinks
    (sink.outline, sink.slice, …) only declare inputPorts in the JSON,
    so the output branch is a no-op for them."""

    def deserialize(self, data: dict) -> bool:
        for name, entry in (data.get('inputPorts') or {}).items():
            if self.input_port(name) is None:
                accepted = entry.get('type', ['ImageData'])
                self.add_input(name, accepted)
        for name, entry in (data.get('outputPorts') or {}).items():
            if self.output_port(name) is None:
                self.add_output(
                    name, entry.get('type', 'ImageData'),
                    persistent=bool(entry.get('persistent', True)))
        return super().deserialize(data)


def _make_inert_sink() -> SinkNode:
    return _InertSink()
