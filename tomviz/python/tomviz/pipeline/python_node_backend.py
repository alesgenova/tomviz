###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Shared backend for schema-v2 Python source / transform nodes in the CLI
runtime. Mirrors the C++ ``tomviz::pipeline::PythonNodeBackend``: owns the
JSON description, the script source, and the current parameter values; runs
the user's ``tomviz.nodes.SourceNode`` / ``tomviz.nodes.TransformNode``
subclass when the host's shell calls ``run_source`` / ``run_transform``.

The shells (``PythonSource`` in ``sources/python_source.py`` and
``PythonTransform`` in ``transforms/python_transform.py``) are thin —
they own one of these and forward."""

from __future__ import annotations

import copy
import importlib.util
import inspect
import json
import logging
import os
import tempfile
from typing import Any, Callable

# Some user scripts reach into tomviz.utils.* without importing it
# (legacy carry-over).
import tomviz.utils  # noqa: F401

from tomviz._internal import OperatorWrapper
from tomviz.pipeline.node import PortData


logger = logging.getLogger('tomviz.pipeline')


def _coerce_param_default(param_type: str, default):
    if isinstance(default, list):
        return list(default)
    if param_type == 'double':
        return float(default) if default is not None else 0.0
    if param_type in ('int', 'integer', 'enumeration'):
        return int(default) if default is not None else 0
    if param_type in ('bool', 'boolean'):
        return bool(default)
    if param_type in ('string', 'file', 'save_file', 'directory'):
        return str(default) if default is not None else ''
    return default


def _resolve_enum_default(param: dict):
    """For an enumeration parameter, return the value of the option at
    the index given by ``default``, or ``None`` if the description is
    malformed."""
    options = param.get('options') or []
    default = param.get('default')
    if not isinstance(default, int) or not (0 <= default < len(options)):
        return None
    opt = options[default]
    if not isinstance(opt, dict) or not opt:
        return None
    return next(iter(opt.values()))


def _find_node_class(module, base_class):
    """Locate a single subclass of ``base_class`` in ``module``. Returns
    ``None`` when no subclass is present; raises ``ValueError`` when more
    than one is found (mirrors the C++
    ``PythonNodeUtils::findNodeClass`` contract)."""
    found = None
    for _, cls in inspect.getmembers(module, inspect.isclass):
        if cls is base_class:
            continue
        if not issubclass(cls, base_class):
            continue
        if found is not None:
            raise ValueError(
                'Multiple node classes defined in module — only one '
                'node class can be defined per script.')
        found = cls
    return found


class PythonNodeBackend:
    """Owns the schema-v2 Python state and runs the user's class."""

    def __init__(self):
        self.json_description: str = ''
        self.script: str = ''
        self.operator_name: str = ''
        self.default_label: str = ''
        self.description_text: str = ''
        self.help_text: str = ''
        self.custom_widget_id: str = ''
        self.supports_cancel: bool = False
        self.supports_complete: bool = False
        self.external_python_env_path: str = ''
        # List of (name, port_type) for inputs.
        self._inputs: list[tuple[str, str]] = []
        # List of (name, port_type, persistent) for outputs.
        self._outputs: list[tuple[str, str, bool]] = []
        self.parameters: dict[str, Any] = {}
        self._parameter_types: dict[str, str] = {}
        self._enum_options: dict[str, list] = {}

    # ---- accessors ----------------------------------------------------

    def primary_output_name(self) -> str:
        return self._outputs[0][0] if self._outputs else ''

    def is_transform_shape(self) -> bool:
        return bool(self._inputs)

    # ---- description / script -----------------------------------------

    def set_json_description(self, json_str: str):
        self.json_description = json_str
        self._parse_description()

    def set_script(self, script: str):
        self.script = script

    def _parse_description(self):
        self.operator_name = ''
        self.default_label = ''
        self.description_text = ''
        self.help_text = ''
        self.custom_widget_id = ''
        self.supports_cancel = False
        self.supports_complete = False
        self.external_python_env_path = ''
        self._inputs = []
        self._outputs = []
        self.parameters = {}
        self._parameter_types = {}
        self._enum_options = {}

        if not self.json_description:
            return
        try:
            obj = json.loads(self.json_description)
        except json.JSONDecodeError:
            logger.exception('Failed to parse operator JSON description')
            return
        if not isinstance(obj, dict):
            return

        self.operator_name = obj.get('name', '')
        self.default_label = obj.get('label', '')
        self.description_text = obj.get('description', '')
        self.help_text = obj.get('help', '')
        self.custom_widget_id = obj.get('widget', '')
        self.supports_cancel = bool(obj.get('supportsCancel', False))
        self.supports_complete = bool(obj.get('supportsComplete', False))
        self.external_python_env_path = obj.get('tomviz_pipeline_env', '')

        for entry in obj.get('inputs', []) or []:
            name = entry.get('name')
            ptype = entry.get('type')
            if name and ptype:
                self._inputs.append((name, ptype))
        for entry in obj.get('outputs', []) or []:
            name = entry.get('name')
            ptype = entry.get('type')
            persistent = bool(entry.get('persistent', False))
            if name and ptype:
                self._outputs.append((name, ptype, persistent))

        for param in obj.get('parameters', []) or []:
            name = param.get('name')
            ptype = param.get('type', '')
            if not name:
                continue
            self._parameter_types[name] = ptype
            if ptype == 'enumeration':
                options = param.get('options') or []
                self._enum_options[name] = options
                resolved = _resolve_enum_default(param)
                if resolved is not None:
                    self.parameters[name] = resolved
                    continue
            if 'default' not in param:
                continue
            self.parameters[name] = _coerce_param_default(
                ptype, param['default'])

    # ---- port creation -------------------------------------------------

    def apply_description(
            self,
            add_input: Callable[[str, str], None] | None,
            add_output: Callable[[str, str, bool], None] | None):
        if add_input:
            for name, ptype in self._inputs:
                add_input(name, ptype)
        if add_output:
            for name, ptype, persistent in self._outputs:
                add_output(name, ptype, persistent)

    # ---- serialize / deserialize --------------------------------------

    def serialize_into(self, base: dict) -> dict:
        base['description'] = self.json_description
        base['script'] = self.script
        if self.parameters:
            base['arguments'] = dict(self.parameters)
        return base

    def apply_serialized_fields(
            self, data: dict,
            add_input: Callable[[str, str], None] | None,
            add_output: Callable[[str, str, bool], None] | None):
        if 'description' in data:
            self.set_json_description(data['description'])
        if 'script' in data:
            self.script = data.get('script', '')
        self.apply_description(add_input, add_output)
        for key, value in (data.get('arguments') or {}).items():
            self.parameters[key] = value

    # ---- execution ----------------------------------------------------

    def run_transform(self, host, inputs: dict[str, PortData]
                      ) -> dict[str, PortData]:
        return self._run_impl(host, inputs, is_source=False)

    def run_source(self, host) -> dict[str, PortData]:
        return self._run_impl(host, {}, is_source=True)

    def _run_impl(self, host, inputs: dict[str, PortData],
                  is_source: bool) -> dict[str, PortData]:
        from tomviz import nodes
        base_class = nodes.SourceNode if is_source else nodes.TransformNode

        module = self._load_script_module()
        if module is None:
            return {}

        try:
            user_class = _find_node_class(module, base_class)
        except ValueError:
            logger.exception(
                'Multiple %s subclasses found in script for %s',
                'SourceNode' if is_source else 'TransformNode',
                self.operator_name)
            return {}
        if user_class is None:
            logger.error(
                'No %s subclass found in script for %s',
                'SourceNode' if is_source else 'TransformNode',
                self.operator_name)
            return {}

        # Instantiate; tomviz.nodes.Node.__new__ pre-attaches a
        # tomviz.operators.Progress(self), which we then overwrite with
        # the CLI's progress object so `self.progress.value = X` lands
        # in the parent-bound progress channel directly (bypassing the
        # _operator_wrapper indirection that's unused on the CLI side).
        instance = user_class()
        progress = getattr(host, 'progress', None)
        if progress is not None:
            instance.progress = progress
            channel = (progress.control_channel()
                       if hasattr(progress, 'control_channel') else None)
            instance._operator_wrapper = OperatorWrapper(channel)
            if hasattr(progress, 'set_primary_port'):
                progress.set_primary_port(self.primary_output_name())
        else:
            instance._operator_wrapper = OperatorWrapper(None)

        kwargs = dict(self.parameters)

        try:
            if is_source:
                result = instance.produce(**kwargs)
            else:
                # Deep-copy each input payload so user mutations don't
                # leak back into upstream port state. Mirrors the C++
                # backend's portDataToPython(deep-copy) behavior.
                inputs_dict = {
                    name: copy.deepcopy(pd.payload)
                    for name, pd in inputs.items()
                }
                result = instance.transform(inputs_dict, **kwargs)
        except Exception:
            logger.exception("Operator '%s' raised", self.operator_name)
            return {}

        # None is the documented signal for "cancel or error" per
        # tomviz.nodes — the user's transform/produce returned without
        # producing outputs. Any other non-dict is treated the same way.
        if not isinstance(result, dict):
            return {}

        outputs: dict[str, PortData] = {}
        for name, ptype, _ in self._outputs:
            if name in result:
                outputs[name] = PortData(result[name], ptype)
        return outputs

    def _load_script_module(self):
        """Materialize self.script as a temp .py and import it. Returns
        the module, or ``None`` on failure."""
        fd = None
        path = None
        try:
            fd, path = tempfile.mkstemp(suffix='.py', text=True)
            os.write(fd, self.script.encode())
            os.close(fd)
            fd = None
            label = (self.operator_name
                     or self.default_label
                     or 'NodeModule')
            spec = importlib.util.spec_from_file_location(label, path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module
        except Exception:
            logger.exception(
                'Failed to load script for %s', self.operator_name)
            return None
        finally:
            if fd is not None:
                os.close(fd)
            if path is not None:
                try:
                    os.unlink(path)
                except OSError:
                    pass
