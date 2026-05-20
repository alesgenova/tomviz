import warnings

import numpy as np
import pytest

from utils import load_operator_module, load_node_class


def test_produce_basic():
    """Test that produce generates non-zero output."""
    module = load_operator_module('RandomParticles')
    node = load_node_class(module)

    result = node.produce(shape=[16, 16, 16])
    ds = result['volume']
    array = ds.active_scalars

    assert not np.allclose(array, 0), "Output should not be all zeros"
    assert np.amax(array) == pytest.approx(1.0, abs=0.01), \
        "Max value should be approximately 1.0 after normalization"


def test_produce_shape():
    """Test that output shape matches requested shape."""
    module = load_operator_module('RandomParticles')
    node = load_node_class(module)

    shape = [20, 24, 18]
    result = node.produce(shape=shape)
    ds = result['volume']
    array = ds.active_scalars

    assert array.shape == tuple(shape)


def test_produce_sparsity():
    """Test that sparsity parameter controls fraction of zero voxels."""
    module = load_operator_module('RandomParticles')
    node = load_node_class(module)

    sparsity = 0.1
    result = node.produce(shape=[32, 32, 32], sparsity=sparsity)
    ds = result['volume']
    array = ds.active_scalars

    zero_fraction = np.count_nonzero(array == 0) / array.size
    assert zero_fraction > 0.8, \
        f"Expected ~90% zeros with sparsity=0.1, got {zero_fraction*100:.1f}%"


def test_produce_uses_int64():
    """Test that no numpy deprecation warnings are raised."""
    module = load_operator_module('RandomParticles')
    node = load_node_class(module)

    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always", DeprecationWarning)
        node.produce(shape=[16, 16, 16])

    numpy_deprecation_warnings = [
        x for x in w
        if issubclass(x.category, DeprecationWarning) and 'numpy' in str(x.message).lower()
    ]
    assert len(numpy_deprecation_warnings) == 0, \
        f"Got numpy deprecation warnings: {[str(x.message) for x in numpy_deprecation_warnings]}"
