# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""User-facing base classes for schema-v2 Python nodes.

A schema-v2 operator script defines exactly one subclass of
:class:`SourceNode` or :class:`TransformNode`. The C++ side
(``PythonSource`` / ``PythonTransform``) instantiates it, injects an
``_operator_wrapper`` for progress / cancel / completion plumbing, and
calls the user method:

  * ``SourceNode.produce(self, **params) -> dict | None`` — for nodes
    with no inputs that emit output port data (file readers, synthetic
    generators).

  * ``TransformNode.transform(self, inputs, **params) -> dict | None``
    — for nodes that consume an ``inputs`` dict and return an outputs
    dict.

Both methods return a dict mapping output port names to payloads, or
``None`` to signal that no output was produced (cancellation, error,
or any condition where the node legitimately has nothing to emit). A
``None`` return is treated by the framework as an execution failure;
downstream nodes are not advanced.

For ImageData / Volume / TiltSeries ports the payload is a
:class:`tomviz.dataset.Dataset`-compatible object; for Table /
Molecule ports it's the corresponding ``vtk`` object.

Progress / cancel / completion follow the existing ``tomviz.operators``
contract — :class:`tomviz.operators.Progress` is reused under the hood
so the multi-port routing of ``self.progress.data = X`` Just Works.
"""

from tomviz._internal import in_application
from tomviz.operators import Progress


class Node:
    """Base class for all schema-v2 tomviz nodes.

    Provides ``self.progress``, ``self.canceled``, and
    ``self.completed``. Don't subclass directly — subclass
    :class:`SourceNode` or :class:`TransformNode` instead so the C++
    factory can dispatch correctly.
    """

    def __new__(cls, *args, **kwargs):
        """:meta private:"""
        obj = super().__new__(cls)
        obj.progress = Progress(obj)
        return obj

    @property
    def canceled(self) -> bool:
        """True when the user has requested cancellation. Long-running
        ``produce`` / ``transform`` implementations should poll this and
        bail out when set."""
        return self._operator_wrapper.canceled

    @property
    def completed(self) -> bool:
        """True when the user has requested early completion (e.g. an
        iterative algorithm should stop with its current best result).
        Iterative ``produce`` / ``transform`` implementations should
        poll this and return what they have."""
        return self._operator_wrapper.completed

    @staticmethod
    def create_dataset() -> 'Dataset':  # noqa: F821
        """Return a new empty :class:`tomviz.dataset.Dataset` of the
        runtime-appropriate concrete class.

        In-app the result is a VTK-backed dataset (so the C++ port-data
        extractor can recover the underlying ``vtkImageData``); in CLI
        and external-executor runs it's the pure-numpy variant. Source
        nodes that need to emit a fresh dataset should call this and
        populate the result via the abstract Dataset API
        (``set_scalars``, ``spacing``, ...). The script then runs
        unchanged across runtimes.
        """
        if in_application():
            from tomviz.internal_dataset import Dataset
        else:
            from tomviz.external_dataset import Dataset
        return Dataset()


class SourceNode(Node):
    """A node that produces output port data without consuming inputs.

    Override :meth:`produce` to compute the outputs.

    .. code-block:: python

        import tomviz.nodes

        class MySource(tomviz.nodes.SourceNode):
            def produce(self, radius=1.0):
                return {'volume': make_sphere(radius)}
    """

    def produce(self, **params) -> dict | None:
        """Compute and return this source's outputs.

        :param params: parameter values, named per the operator JSON
            description's ``parameters`` array.
        :returns: dict mapping output port names (declared in the JSON
            ``outputs`` array) to payload objects. Return ``None``
            (or simply ``return`` with no value) to signal cancellation
            or error — the framework treats it as no output produced.

        :meta private:
        """
        raise NotImplementedError(
            f'{type(self).__name__}.produce is not implemented')


class TransformNode(Node):
    """A node that consumes inputs and produces outputs.

    Override :meth:`transform` to compute the outputs from the inputs.

    .. code-block:: python

        import tomviz.nodes

        class AddConstant(tomviz.nodes.TransformNode):
            def transform(self, inputs, constant=0.0):
                ds = inputs['volume']
                ds.active_scalars = ds.active_scalars + constant
                return {'volume': ds}
    """

    def transform(self, inputs: dict, **params) -> dict | None:
        """Compute and return this transform's outputs.

        :param inputs: dict mapping input port names (declared in the
            JSON ``inputs`` array) to payload objects.
        :param params: parameter values, named per the operator JSON
            description's ``parameters`` array.
        :returns: dict mapping output port names (declared in the JSON
            ``outputs`` array) to payload objects. Return ``None``
            (or simply ``return`` with no value) to signal cancellation
            or error — the framework treats it as no output produced.

        :meta private:
        """
        raise NotImplementedError(
            f'{type(self).__name__}.transform is not implemented')
