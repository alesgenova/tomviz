# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

"""VTK-backed Dataset implementations for tomviz operators running
inside the application or the new pipeline.

:class:`Dataset` is the base in-app Dataset — it wraps a vtkImageData
and exposes the standard scalar / spacing / tilt-series API used by
schema-v2 (``tomviz.nodes``) operators. :class:`LegacyDataset` adds the
``create_child_dataset`` shim used by the v1 ``tomviz.operators``
operators (recon-style ops that emit a child volume); v1 operators
running under ``LegacyPythonTransform`` get instances of this subclass.

The CLI / external-execution path uses :class:`tomviz.external_dataset.Dataset`
instead — pure numpy, no VTK dep."""

import numpy as np
import vtk.numpy_interface.dataset_adapter as dsa
import vtk.util.numpy_support as np_s

from tomviz.dataset import Dataset as AbstractDataset


_DEFAULT_SCALARS_NAME = 'Scalars'


class Dataset(AbstractDataset):
    """VTK-backed Dataset wrapping a vtkImageData. Used by schema-v2
    operators (``tomviz.nodes.SourceNode`` / ``TransformNode``) and by
    any in-app pipeline node that hands a Python script a vtkImageData
    payload."""

    def __init__(self, data_object=None):
        if data_object is None:
            from vtk import vtkImageData
            data_object = vtkImageData()
        self._data_object = data_object
        self._tilt_axis = 2
        # Used by active_scalars setter on a still-empty vtkImageData
        # so the array doesn't default to "Scalars".
        self._inherited_active_name: str | None = None

    @property
    def active_scalars(self):
        do = dsa.WrapDataObject(self._data_object)
        rawarray = do.PointData.GetScalars()
        if rawarray is None:
            return None
        vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
        return np.reshape(vtkarray, self._data_object.GetDimensions(), order='F')

    @active_scalars.setter
    def active_scalars(self, v):
        if np.isfortran(v):
            arr = v.reshape(-1, order='F')
            vtkshape = v.shape
        else:
            vtkshape = v.shape
            tmp = np.asfortranarray(v)
            arr = tmp.reshape(-1, order='F')

        try:
            np_s.get_vtk_array_type(arr.dtype)
        except TypeError:
            arr = arr.astype(np.float32)

        do = dsa.WrapDataObject(self._data_object)
        oldscalars = do.PointData.GetScalars()
        if oldscalars is not None:
            arrayname = oldscalars.GetName()
        elif self._inherited_active_name:
            arrayname = self._inherited_active_name
        else:
            arrayname = _DEFAULT_SCALARS_NAME

        # Update extent if shape changed
        sameshape = list(vtkshape) == list(self._data_object.GetDimensions())
        if not sameshape:
            extent = [0, vtkshape[0] - 1, 0, vtkshape[1] - 1,
                      0, vtkshape[2] - 1]
            self._data_object.SetExtent(extent)

        do.PointData.append(arr, arrayname)
        do.PointData.SetActiveScalars(arrayname)

    @property
    def active_name(self):
        scalars = self._data_object.GetPointData().GetScalars()
        if scalars is None:
            return None
        return scalars.GetName()

    @active_name.setter
    def active_name(self, name):
        self._data_object.GetPointData().SetActiveScalars(name)

    @property
    def num_scalars(self):
        return len(self.scalars_names)

    @property
    def scalars_names(self):
        do = dsa.WrapDataObject(self._data_object)
        num_arrays = do.PointData.GetNumberOfArrays()
        return [do.PointData.GetArrayName(i) for i in range(num_arrays)]

    def scalars(self, name=None):
        do = dsa.WrapDataObject(self._data_object)
        if name is not None:
            rawarray = do.PointData.GetAbstractArray(name)
        else:
            rawarray = do.PointData.GetScalars()
        if rawarray is None:
            return None
        vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
        return np.reshape(vtkarray, self._data_object.GetDimensions(), order='F')

    def set_scalars(self, name, array):
        if np.isfortran(array):
            arr = array.reshape(-1, order='F')
            vtkshape = array.shape
        else:
            vtkshape = array.shape
            tmp = np.asfortranarray(array)
            arr = tmp.reshape(-1, order='F')

        try:
            np_s.get_vtk_array_type(arr.dtype)
        except TypeError:
            arr = arr.astype(np.float32)

        sameshape = list(vtkshape) == list(self._data_object.GetDimensions())
        if not sameshape:
            extent = [0, vtkshape[0] - 1, 0, vtkshape[1] - 1,
                      0, vtkshape[2] - 1]
            self._data_object.SetExtent(extent)

        do = dsa.WrapDataObject(self._data_object)
        do.PointData.append(arr, name)
        if do.PointData.GetNumberOfArrays() == 1:
            do.PointData.SetActiveScalars(name)

    def remove_scalars(self, name):
        pd = self._data_object.GetPointData()
        if pd.GetAbstractArray(name) is None:
            raise KeyError(f"No scalar array named '{name}'")
        pd.RemoveArray(name)
        if pd.GetScalars() is None and pd.GetNumberOfArrays() > 0:
            pd.SetActiveScalars(pd.GetArrayName(0))

    def rename_active(self, new_name: str):
        scalars = self._data_object.GetPointData().GetScalars()
        if scalars is not None:
            scalars.SetName(new_name)

    def empty_copy(self) -> 'Dataset':
        # Build a fresh vtkImageData with the same geometry (extent,
        # spacing, origin) and field data (tilt_angles, scan_ids,
        # ...) but no point-data arrays — apply_to_each_scalar_array
        # will fill those in. type(self) preserves the concrete
        # subclass (Dataset vs LegacyDataset).
        from vtk import vtkImageData
        new_data = vtkImageData()
        new_data.CopyStructure(self._data_object)
        new_data.GetFieldData().ShallowCopy(
            self._data_object.GetFieldData())
        new_ds = type(self)(new_data)
        new_ds._tilt_axis = self._tilt_axis
        new_ds._inherited_active_name = self._inherited_active_name
        return new_ds

    @property
    def spacing(self):
        return self._data_object.GetSpacing()

    @spacing.setter
    def spacing(self, v):
        self._data_object.SetSpacing(v[0], v[1], v[2])

    @property
    def tilt_angles(self):
        do = dsa.WrapDataObject(self._data_object)
        rawarray = do.FieldData.GetArray('tilt_angles')
        if isinstance(rawarray, dsa.VTKNoneArray):
            return None
        if rawarray is None:
            return None
        vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
        return vtkarray

    @tilt_angles.setter
    def tilt_angles(self, v):
        from vtkmodules.util.vtkConstants import VTK_DOUBLE
        vtkarray = np_s.numpy_to_vtk(v, deep=1, array_type=VTK_DOUBLE)
        vtkarray.SetName('tilt_angles')
        do = dsa.WrapDataObject(self._data_object)
        do.FieldData.RemoveArray('tilt_angles')
        do.FieldData.AddArray(vtkarray)

    @property
    def tilt_axis(self):
        return self._tilt_axis

    @tilt_axis.setter
    def tilt_axis(self, v):
        self._tilt_axis = v

    @property
    def scan_ids(self):
        do = dsa.WrapDataObject(self._data_object)
        rawarray = do.FieldData.GetArray('scan_ids')
        if isinstance(rawarray, dsa.VTKNoneArray):
            return None
        if rawarray is None:
            return None
        vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
        return vtkarray

    @scan_ids.setter
    def scan_ids(self, v):
        from vtkmodules.util.vtkConstants import VTK_INT
        if v is None:
            do = dsa.WrapDataObject(self._data_object)
            do.FieldData.RemoveArray('scan_ids')
            return
        vtkarray = np_s.numpy_to_vtk(v, deep=1, array_type=VTK_INT)
        vtkarray.SetName('scan_ids')
        do = dsa.WrapDataObject(self._data_object)
        do.FieldData.RemoveArray('scan_ids')
        do.FieldData.AddArray(vtkarray)

    @property
    def dark(self):
        return None

    @property
    def white(self):
        return None

    @property
    def file_name(self):
        return None

    @property
    def metadata(self):
        return {}


class LegacyDataset(Dataset):
    """Adds the v1 ``create_child_dataset`` API expected by ``tomviz.
    operators``-derived recon-style operators (which emit a child
    volume as their primary output). LegacyPythonTransform constructs
    instances of this class for v1 scripts; schema-v2 nodes get the
    plain :class:`Dataset` instead, so v2 authors don't see the
    legacy affordance."""

    def create_child_dataset(self):
        from vtk import vtkImageData
        new_child = vtkImageData()
        new_child.CopyStructure(self._data_object)
        input_spacing = self._data_object.GetSpacing()
        child_spacing = (input_spacing[0], input_spacing[1], input_spacing[0])
        new_child.SetSpacing(child_spacing)
        child = LegacyDataset(new_child)
        # Carry the parent's active-scalar name so the child's first
        # array doesn't default to "Scalars".
        parent_active = self._data_object.GetPointData().GetScalars()
        if parent_active is not None and parent_active.GetName():
            child._inherited_active_name = parent_active.GetName()
        return child


def create_dataset(data_object, data_source=None):
    # Legacy entry point invoked by tomviz::Python::createDataset from
    # legacy::OperatorPython::applyTransform. data_source is unused.
    del data_source
    return LegacyDataset(data_object)
