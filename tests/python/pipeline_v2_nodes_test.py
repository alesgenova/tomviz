###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Tests for the schema-v2 Python node runtime — the CLI side of
``tomviz.pipeline.transforms.python_transform.PythonTransform`` and
``tomviz.pipeline.sources.python_source.PythonSource`` — plus the
abstract Dataset helpers (``apply_to_each_scalar_array``,
``empty_copy``) used heavily by v2 operator authors.

The C++ side of the same code paths is covered by the
PipelinePythonTest gtest cases (``PythonTransformV2``, etc.); this
file mirrors them on the Python runtime so the two sides don't drift.
"""

import json

import numpy as np
import pytest

from tomviz.external_dataset import Dataset, LegacyDataset
from tomviz.pipeline import PortData, register_builtins
from tomviz.pipeline.sources.python_source import PythonSource
from tomviz.pipeline.transforms.python_transform import PythonTransform


@pytest.fixture(autouse=True)
def _builtins():
    register_builtins()


def _multi_array_dataset():
    """Build an external Dataset with two scalar arrays so the
    apply_to_each / filter / active-preserve paths can all be
    exercised against the same fixture."""
    ds = Dataset({'a': np.array([1.0, 2.0, 3.0]),
                  'b': np.array([10.0, 20.0, 30.0])}, active='a')
    ds.spacing = (0.5, 0.5, 0.5)
    ds.tilt_axis = 1
    return ds


# ============================================================
# Dataset.apply_to_each_scalar_array (return-new + filter)
# ============================================================


def test_apply_to_each_scalar_array_returns_new_dataset():
    src = _multi_array_dataset()
    out = src.apply_to_each_scalar_array(lambda a: a * 2)

    assert out is not src
    # Source untouched
    np.testing.assert_array_equal(src.arrays['a'], [1.0, 2.0, 3.0])
    np.testing.assert_array_equal(src.arrays['b'], [10.0, 20.0, 30.0])
    # Output transformed
    np.testing.assert_array_equal(out.arrays['a'], [2.0, 4.0, 6.0])
    np.testing.assert_array_equal(out.arrays['b'], [20.0, 40.0, 60.0])


def test_apply_to_each_scalar_array_preserves_metadata():
    src = _multi_array_dataset()
    out = src.apply_to_each_scalar_array(lambda a: a + 1)

    assert tuple(out.spacing) == (0.5, 0.5, 0.5)
    assert out.tilt_axis == 1
    assert out.active_name == 'a'
    assert sorted(out.scalars_names) == ['a', 'b']


def test_apply_to_each_scalar_array_accepts_in_place_mutation():
    src = _multi_array_dataset()

    def in_place(arr):
        arr += 100
        return arr

    out = src.apply_to_each_scalar_array(in_place)
    np.testing.assert_array_equal(out.arrays['a'], [101.0, 102.0, 103.0])


def test_apply_to_each_scalar_array_filters_on_none():
    src = _multi_array_dataset()
    # Drop 'b' by returning None for it.
    out = src.apply_to_each_scalar_array(
        lambda a: None if a[0] == 10.0 else a * 2)

    assert out.scalars_names == ['a']
    np.testing.assert_array_equal(out.arrays['a'], [2.0, 4.0, 6.0])
    assert out.active_name == 'a'


def test_apply_to_each_scalar_array_filters_active_falls_back():
    """When fn filters out the active array, the next remaining
    scalar should become the new active."""
    src = _multi_array_dataset()
    out = src.apply_to_each_scalar_array(
        lambda a: None if a[0] == 1.0 else a)

    assert out.scalars_names == ['b']
    assert out.active_name == 'b'


def test_apply_to_each_scalar_array_filters_all_returns_empty():
    src = _multi_array_dataset()
    out = src.apply_to_each_scalar_array(lambda a: None)

    assert out.scalars_names == []
    # Source untouched
    assert sorted(src.scalars_names) == ['a', 'b']


def test_apply_to_each_scalar_array_preserves_concrete_type():
    """LegacyDataset in → LegacyDataset out (so chained v1 operators
    keep create_child_dataset access)."""
    src = LegacyDataset({'a': np.array([1.0])}, active='a')
    out = src.apply_to_each_scalar_array(lambda a: a * 2)
    assert isinstance(out, LegacyDataset)


# ============================================================
# Dataset.empty_copy
# ============================================================


def test_empty_copy_preserves_metadata_drops_arrays():
    src = _multi_array_dataset()
    out = src.empty_copy()

    assert out is not src
    assert out.scalars_names == []
    assert tuple(out.spacing) == (0.5, 0.5, 0.5)
    assert out.tilt_axis == 1
    # Source untouched
    assert sorted(src.scalars_names) == ['a', 'b']


def test_empty_copy_preserves_concrete_type():
    src = LegacyDataset({'a': np.array([1.0])}, active='a')
    out = src.empty_copy()
    assert isinstance(out, LegacyDataset)


# ============================================================
# PythonTransform CLI runtime: end-to-end execute
# ============================================================


def _multiply_v2_description(persistent=True):
    return json.dumps({
        'schemaVersion': 2,
        'name': 'MultiplyBy',
        'label': 'Multiply By',
        'inputs':  [{'name': 'volume', 'type': 'ImageData'}],
        'outputs': [{'name': 'volume', 'type': 'ImageData',
                     'persistent': persistent}],
        'parameters': [{'name': 'factor', 'type': 'double',
                        'default': 1.0}],
    })


_MULTIPLY_V2_SCRIPT = """
import tomviz.nodes


class MultiplyBy(tomviz.nodes.TransformNode):
    def transform(self, inputs, factor=1.0):
        ds = inputs["volume"]
        return {"volume": ds.apply_to_each_scalar_array(lambda a: a * factor)}
"""


def test_python_transform_v2_executes_end_to_end():
    transform = PythonTransform()
    transform.set_json_description(_multiply_v2_description())
    transform.script = _MULTIPLY_V2_SCRIPT
    transform._backend.parameters['factor'] = 3.0

    src = Dataset({'a': np.array([1.0, 2.0, 3.0])}, active='a')
    inputs = {'volume': PortData(src, 'ImageData')}

    outputs = transform.transform(inputs)
    assert 'volume' in outputs
    out_ds = outputs['volume'].payload
    np.testing.assert_array_equal(out_ds.arrays['a'], [3.0, 6.0, 9.0])
    # Apply-helper return-new: source unchanged.
    np.testing.assert_array_equal(src.arrays['a'], [1.0, 2.0, 3.0])


def test_python_transform_v2_label_and_ports_from_json():
    """Setting the JSON description should populate the host's label
    and create the input/output ports."""
    transform = PythonTransform()
    transform.set_json_description(_multiply_v2_description())

    assert transform.label == 'Multiply By'
    assert [p.name for p in transform.input_ports()] == ['volume']
    assert [p.name for p in transform.output_ports()] == ['volume']


def test_python_transform_v2_none_return_signals_failure():
    """A transform that returns None should produce no outputs — the
    backend collapses non-dict (including None) to an empty result."""
    transform = PythonTransform()
    transform.set_json_description(_multiply_v2_description())
    transform.script = """
import tomviz.nodes

class Refuse(tomviz.nodes.TransformNode):
    def transform(self, inputs, factor=1.0):
        return  # None — signals "no output produced"
"""

    src = Dataset({'a': np.array([1.0])}, active='a')
    inputs = {'volume': PortData(src, 'ImageData')}
    outputs = transform.transform(inputs)
    assert outputs == {}


def test_python_transform_v2_exception_signals_failure():
    """A transform that raises should be caught and produce no
    outputs (logged at the runtime)."""
    transform = PythonTransform()
    transform.set_json_description(_multiply_v2_description())
    transform.script = """
import tomviz.nodes

class Boom(tomviz.nodes.TransformNode):
    def transform(self, inputs, factor=1.0):
        raise RuntimeError("intentional")
"""

    src = Dataset({'a': np.array([1.0])}, active='a')
    inputs = {'volume': PortData(src, 'ImageData')}
    outputs = transform.transform(inputs)
    assert outputs == {}


def test_python_transform_v2_supports_cancel_flag_from_json():
    transform = PythonTransform()
    transform.set_json_description(json.dumps({
        'schemaVersion': 2,
        'name': 'Demo',
        'inputs':  [{'name': 'volume', 'type': 'ImageData'}],
        'outputs': [{'name': 'volume', 'type': 'ImageData'}],
        'supportsCancel': True,
        'supportsComplete': True,
    }))
    assert transform._backend.supports_cancel is True
    assert transform._backend.supports_complete is True


# ============================================================
# PythonSource CLI runtime: end-to-end produce
# ============================================================


def _constant_source_description():
    return json.dumps({
        'schemaVersion': 2,
        'name': 'ConstantSource',
        'label': 'Constant Source',
        'inputs': [],
        'outputs': [{'name': 'volume', 'type': 'ImageData',
                     'persistent': True}],
        'parameters': [{'name': 'value', 'type': 'double',
                        'default': 0.0}],
    })


_CONSTANT_SOURCE_SCRIPT = """
import numpy as np
import tomviz.nodes
from tomviz.external_dataset import Dataset


class ConstantSource(tomviz.nodes.SourceNode):
    def produce(self, value=0.0):
        arr = np.full((2, 2, 2), value, dtype=np.float32)
        return {"volume": Dataset({"Scalars": arr}, active="Scalars")}
"""


def test_python_source_v2_executes_end_to_end():
    source = PythonSource()
    source.set_json_description(_constant_source_description())
    source.script = _CONSTANT_SOURCE_SCRIPT
    source._backend.parameters['value'] = 7.0

    assert source.execute() is True
    port = source.output_port('volume')
    assert port.has_data()
    out_ds = port.data().payload
    assert out_ds.scalars('Scalars').shape == (2, 2, 2)
    np.testing.assert_array_equal(
        out_ds.scalars('Scalars'),
        np.full((2, 2, 2), 7.0, dtype=np.float32))


def test_python_source_v2_none_return_fails_execute():
    source = PythonSource()
    source.set_json_description(_constant_source_description())
    source.script = """
import tomviz.nodes

class Refuse(tomviz.nodes.SourceNode):
    def produce(self, value=0.0):
        return  # None — no output produced
"""
    # outputs declared but produce() returned None → execute() returns
    # False (no outputs produced for declared ports).
    assert source.execute() is False
    assert not source.output_port('volume').has_data()


# ============================================================
# Schema validation
# ============================================================


def test_python_transform_v2_persistent_default_false():
    """When the JSON omits `persistent` for an output, the schema-v2
    convention is `false` (transient) — the backend must call
    setPersistent(false) explicitly."""
    transform = PythonTransform()
    transform.set_json_description(json.dumps({
        'schemaVersion': 2,
        'name': 'Demo',
        'inputs':  [{'name': 'volume', 'type': 'ImageData'}],
        'outputs': [{'name': 'volume', 'type': 'ImageData'}],  # no persistent
    }))
    assert transform.output_port('volume').persistent is False


def test_python_transform_v2_persistent_explicit_true():
    transform = PythonTransform()
    transform.set_json_description(json.dumps({
        'schemaVersion': 2,
        'name': 'Demo',
        'inputs':  [{'name': 'volume', 'type': 'ImageData'}],
        'outputs': [{'name': 'volume', 'type': 'ImageData',
                     'persistent': True}],
    }))
    assert transform.output_port('volume').persistent is True
