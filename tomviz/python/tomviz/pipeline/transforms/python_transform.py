###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""PythonTransform — schema-v2 Python transform node for the CLI runtime.

Mirrors the C++ ``tomviz::pipeline::PythonTransform``: a thin shell that
owns a :class:`PythonNodeBackend` and delegates all parsing /
serialization / execution to it. The user-facing class is a subclass of
:class:`tomviz.nodes.TransformNode` defined in the operator script."""

from tomviz.pipeline.node import PortData, TransformNode
from tomviz.pipeline.python_node_backend import PythonNodeBackend


class PythonTransform(TransformNode):
    type_name = 'transform.python'

    def __init__(self):
        super().__init__()
        self._backend = PythonNodeBackend()

    # ---- description / script -----------------------------------------

    def set_json_description(self, json_str: str):
        self._backend.set_json_description(json_str)
        if self._backend.default_label:
            self.label = self._backend.default_label
        self._backend.apply_description(
            lambda name, ptype: self.add_input(name, ptype),
            lambda name, ptype, persistent: self.add_output(
                name, ptype, persistent=persistent))

    @property
    def json_description(self) -> str:
        return self._backend.json_description

    @property
    def script(self) -> str:
        return self._backend.script

    @script.setter
    def script(self, value: str):
        self._backend.set_script(value)

    @property
    def parameters(self) -> dict:
        return self._backend.parameters

    # ---- serialize / deserialize --------------------------------------

    def deserialize(self, data: dict) -> bool:
        # Apply description first so add_input / add_output run before
        # the base class restores per-port state.
        self._backend.apply_serialized_fields(
            data,
            lambda name, ptype: (
                self.add_input(name, ptype)
                if self.input_port(name) is None else None),
            lambda name, ptype, persistent: (
                self.add_output(name, ptype, persistent=persistent)
                if self.output_port(name) is None else None))
        return super().deserialize(data)

    # ---- execution ----------------------------------------------------

    def transform(self, inputs: dict[str, PortData]) -> dict[str, PortData]:
        return self._backend.run_transform(self, inputs)
