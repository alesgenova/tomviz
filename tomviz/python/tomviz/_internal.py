# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
from types import MethodType
from typing import Any, Callable
import fnmatch
import inspect
import json
import os
import sys
import traceback

import tomviz
import tomviz.operators


def in_application():
    return os.environ.get('TOMVIZ_APPLICATION', False)


if in_application():
    import tomviz._wrapping
    from vtk import vtkDataObject


def require_internal_mode():
    if not in_application():
        func_name = str(inspect.currentframe().f_back.f_code.co_name)
        raise Exception('Cannot call ' + func_name + ' in external mode')


def delete_module(name):
    if name in sys.modules:
        del sys.modules[name]


class OperatorWrapper(object):
    """Backs `self.canceled` / `self.completed` on operators running
    under the pure-Python pipeline runtime. The flags can be flipped
    out-of-band by the parent process via a transport-specific
    ControlChannel, which is polled lazily on every getter access —
    no reader thread needed on the subprocess side."""

    def __init__(self, control_channel=None):
        self._channel = control_channel
        self._canceled = False
        self._completed = False

    @property
    def canceled(self) -> bool:
        if self._channel is not None:
            self._channel.poll(self)
        return self._canceled

    @property
    def completed(self) -> bool:
        if self._channel is not None:
            self._channel.poll(self)
        return self._completed


def find_operator_class(transform_module):
    operator_class = None
    classes = inspect.getmembers(transform_module, inspect.isclass)
    for (name, cls) in classes:
        if issubclass(cls, tomviz.operators.Operator):
            if operator_class is not None:
                raise Exception('Multiple operators define in module, only '
                                'one operator can be defined per module.')

            operator_class = cls

    return operator_class


def _find_function(module, function_name):
    # Finds a function in the module with a given "function_name"
    # Returns `None` if it is not found
    functions = inspect.getmembers(module, inspect.isfunction)
    for (name, func) in functions:
        if name == function_name:
            return func


def find_transform_from_module(transform_module):
    # This tries to first find transform(), and then transform_scalars()
    f = _find_function(transform_module, 'transform')
    if f is None:
        f = _find_function(transform_module, 'transform_scalars')

    return f


def is_cancelable(transform_module):
    cls = find_operator_class(transform_module)

    if cls is None:
        function = find_transform_from_module(transform_module)

    if cls is None and function is None:
        raise Exception('Unable to locate function or operator class.')

    return cls is not None and issubclass(cls,
                                          tomviz.operators.CancelableOperator)


def is_completable(transform_module):
    cls = find_operator_class(transform_module)

    if cls is None:
        function = find_transform_from_module(transform_module)
        if not function:
            raise Exception('Unable to locate function or operator class.')

    return cls is not None and issubclass(
        cls,
        tomviz.operators.CompletableOperator
    )


def find_transform_function(transform_module, op=None):
    # op is accepted for ABI compat with legacy OperatorPython but ignored.
    del op

    transform_function = find_transform_from_module(transform_module)
    if transform_function is None:
        cls = find_operator_class(transform_module)
        if cls is None:
            raise Exception('Unable to locate transform function.')

        o = cls.__new__(cls)
        # _operator_wrapper is read by CompletableOperator/CancelableOperator
        # __init__ and during transform(); install the pure-Python fallback.
        o._operator_wrapper = OperatorWrapper(None)
        cls.__init__(o)

        transform_function = None
        if _operator_method_was_implemented(o, 'transform'):
            transform_function = o.transform
        elif _operator_method_was_implemented(o, 'transform_scalars'):
            transform_function = o.transform_scalars

    if transform_function is None:
        raise Exception('Unable to locate transform function.')

    return transform_function


def has_decorator(func: Callable, decorator_marker: str = '_is_my_decorator') -> bool:
    """Check if a function was already decorated with a decorator name"""
    # Check the function itself
    if getattr(func, decorator_marker, False):
        return True

    # Traverse the __wrapped__ chain
    current = func
    while hasattr(current, '__wrapped__'):
        current = current.__wrapped__
        if getattr(current, decorator_marker, False):
            return True

    return False


def apply_decorator(func: Callable, decorator: Callable) -> Callable:
    # Apply the decorator, taking into account different behavior for MethodType
    # callables
    if isinstance(func, MethodType):
        # It's a bound method
        tmp_func = decorator(func.__func__)
        return MethodType(tmp_func, func.__self__)

    # Unbound function
    return decorator(func)


def add_transform_decorators(transform_method: Callable,
                             operator_dict: dict[str, Any]) -> Callable:
    """Optionally add any transform wrappers that we need to add

    Currently, this adds `@apply_to_each_array` automatically if
    `"apply_to_each_array": false` is not set within the json
    description, and if the decorator was not already applied.
    """
    add_apply_to_each_array = True
    operator_description = operator_dict.get('description')
    if operator_description:
        description_json = json.loads(operator_description)
        if not description_json.get('apply_to_each_array', True):
            # It was intentionally disabled in the json
            add_apply_to_each_array = False

    if transform_method.__name__ == 'transform_scalars':
        # This is an old transform function. We don't want to do any
        # kind of automatic modifications to the old ones.
        add_apply_to_each_array = False

    if add_apply_to_each_array:
        # First, make sure it wasn't already decorated
        if not has_decorator(transform_method, 'apply_to_each_array'):
            # Decorate it!
            from tomviz.utils import apply_to_each_array
            transform_method = apply_decorator(transform_method, apply_to_each_array)

    return transform_method


def transform_method_wrapper(transform_method: Callable,
                             operator_serialized: str, *args, **kwargs):
    # External execution is now driven from C++ via the per-node
    # ExternalNodeExecutor strategy (see tomviz/pipeline/
    # ExternalNodeExecutor.h). The wrapper still receives the serialized
    # operator JSON for symmetry with the legacy entry point and so any
    # future Python-side dispatch decisions have it on hand, but the
    # `tomviz_pipeline_env` branch was removed: when it's set on the
    # operator description, LegacyPythonTransform synthesizes a
    # NodeExecutor at deserialize time and the in-app run happens out of
    # process before Python is even involved.
    operator_dict = json.loads(operator_serialized)
    transform_method = add_transform_decorators(transform_method, operator_dict)
    return transform_method(*args, **kwargs)


def _classify_from_json(operator_json):
    # Schema v2 with non-empty inputs is a transform; schema v2 without inputs
    # is a source. Anything else (no schemaVersion, or v1) is a legacy
    # transform.
    if operator_json.get('schemaVersion') == 2:
        if operator_json.get('inputs'):
            return 'transform'
        return 'source'
    return 'legacy_transform'


def _operator_description(operator_dir, filename):
    name, _ = os.path.splitext(filename)
    description = {
        'label': name,
        'pythonPath': os.path.join(operator_dir, filename),
        'valid': True,
        'type': 'legacy_transform',
    }

    json_filepath = os.path.join(operator_dir, '%s.json' % name)
    if os.path.exists(json_filepath):
        description['jsonPath'] = json_filepath
        try:
            with open(json_filepath) as fp:
                operator_json = json.load(fp)
            description['label'] = operator_json.get('label', name)
            description['type'] = _classify_from_json(operator_json)
        except Exception:
            description['loadError'] = traceback.format_exc()
            description['valid'] = False

    return description


def find_operators(operator_dir):
    # First look for the python files
    python_files = fnmatch.filter(os.listdir(operator_dir), '*.py')
    operator_descriptions = []
    for python_file in python_files:
        operator_descriptions.append(
            _operator_description(operator_dir, python_file)
        )

    return operator_descriptions


def _operator_method_was_implemented(obj, method):
    # It would be nice if there were an easier way to do this, but
    # I am not currently aware of an easier way.
    bases = list(inspect.getmro(type(obj)))
    # We know operator has this attribute, remove it
    bases.remove(tomviz.operators.Operator)

    for base in bases:
        if method in vars(base):
            return True

    return False


def convert_to_dataset(data):
    # This method will extract/convert certain data types to a dataset

    if in_application():
        from tomviz.internal_dataset import Dataset
    else:
        from tomviz.external_dataset import Dataset

    if isinstance(data, Dataset):
        # It is already a dataset
        return data

    if in_application():
        if isinstance(data, vtkDataObject):
            # Wrap the bare VTK object in a Dataset.
            return Dataset(data)

    msg = 'Cannot convert type to Dataset: ' + str(type(data))
    raise Exception(msg)


def convert_to_vtk_data_object(data):
    # This method will extract/convert certain data types to a vtkDataObject

    from tomviz.dataset import Dataset

    if isinstance(data, vtkDataObject):
        # It is already a vtkDataObject
        return data

    if isinstance(data, Dataset):
        # Should be stored in _data_object
        return data._data_object

    msg = 'Cannot convert type to vtkDataObject: ' + str(type(data))
    raise Exception(msg)


def with_vtk_dataobject(f):
    # A decorator to automatically convert the first argument to
    # a vtkDataObject. This also confirms we are running internally.

    def wrapped(*args, **kwargs):
        if not in_application():
            name = f.__name__
            raise Exception('Cannot call ' + name + ' in external mode')

        dataobject = convert_to_vtk_data_object(args[0])
        args = (dataobject, *args[1:])
        return f(*args, **kwargs)

    return wrapped


def with_dataset(f):
    # A decorator to automatically convert the first argument to
    # a Dataset.

    def wrapped(*args, **kwargs):
        dataset = convert_to_dataset(args[0])
        args = (dataset, *args[1:])
        return f(*args, **kwargs)

    return wrapped
