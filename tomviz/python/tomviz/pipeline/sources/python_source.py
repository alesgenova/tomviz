###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""PythonSource — schema-v2 Python source node for the CLI runtime.

Mirrors the C++ ``tomviz::pipeline::PythonSource``. The user-facing class
is a subclass of :class:`tomviz.nodes.SourceNode` whose ``produce``
method returns the outputs dict."""

from tomviz.pipeline.node import SourceNode
from tomviz.pipeline.python_node_backend import PythonNodeBackend


class PythonSource(SourceNode):
    type_name = 'source.python'

    def __init__(self):
        super().__init__()
        self._backend = PythonNodeBackend()

    # ---- description / script -----------------------------------------

    def set_json_description(self, json_str: str):
        self._backend.set_json_description(json_str)
        if self._backend.default_label:
            self.label = self._backend.default_label
        # Sources have no inputs — pass a null AddInputFn. A v2 source
        # description that erroneously declares inputs is silently
        # dropped here; the menu-routing reaction in the app rejects
        # the same case at construction time.
        self._backend.apply_description(
            None,
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
        self._backend.apply_serialized_fields(
            data,
            None,
            lambda name, ptype, persistent: (
                self.add_output(name, ptype, persistent=persistent)
                if self.output_port(name) is None else None))
        return super().deserialize(data)

    # ---- execution ----------------------------------------------------

    def execute(self) -> bool:
        outputs = self._backend.run_source(self)
        if not outputs and self.output_ports():
            return False
        for name, port_data in outputs.items():
            port = self.output_port(name)
            if port is not None:
                port.set_data(port_data)
        return True
