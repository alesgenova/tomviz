from abc import ABC, abstractmethod
from typing import Callable

import numpy as np


class Dataset(ABC):
    """Abstract base class for tomviz Dataset objects.

    The Dataset is the standard object that is passed to operators within tomviz.
    It provides a unified interface for accessing and manipulating tilt image
    stacks and volumetric data, including scalar arrays, spacing information,
    tilt series metadata (such as tilt angles), and calibration data
    (dark/white fields).

    This object will always be automatically provided as the first argument in
    the `transform()` function within operators. For example, for the
    `Rotate` operator:

    .. code-block:: python

        def transform(dataset, rotation_angle=90.0, rotation_axis=0):
            # ...

            array = dataset.active_scalars

            # ...

    .. note::
       Concrete subclasses of this abstract base class are internal to tomviz.
       This documentation describes the public API that is accessible on
       Dataset instances passed to operators and other user-facing code.
       Users should interact with Dataset objects through this interface
       rather than instantiating subclasses directly.
    """
    @property
    @abstractmethod
    def active_scalars(self) -> np.ndarray:
        """The currently active scalar array data as a numpy array.

        The shape and dtype depend on the specific dataset.
        """
        pass

    @active_scalars.setter
    @abstractmethod
    def active_scalars(self, v: np.ndarray):
        """Set the currently active scalar array data."""
        pass

    @property
    @abstractmethod
    def active_name(self) -> str:
        """The name of the currently active scalar array."""
        pass

    @active_name.setter
    @abstractmethod
    def active_name(self, name: str):
        """Set which scalar array is the active one (by name)."""
        pass

    @property
    @abstractmethod
    def num_scalars(self) -> int:
        """The total number of scalar arrays stored in this dataset."""
        pass

    @property
    @abstractmethod
    def scalars_names(self) -> list[str]:
        """List containing the name of each scalar array in the dataset."""
        pass

    @abstractmethod
    def scalars(self, name: str | None = None) -> np.ndarray:
        """Get a scalar array by name.

        :param name: The name of the scalar array to retrieve. If None, returns the
                     active scalar array.
        :return: The requested scalar array data.
        :raises KeyError: If the specified name does not exist in the dataset.
        """
        pass

    @abstractmethod
    def set_scalars(self, name: str, array: np.ndarray):
        """Add or update a scalar array in the dataset.

        :param name: The name to assign to this scalar array. If a field with this name
                     already exists, it will be overwritten.
        :param array: The scalar array data to store.
        """
        pass

    @abstractmethod
    def remove_scalars(self, name: str):
        """Remove a scalar array from the dataset.

        :param name: The name of the scalar array to remove.
        :raises KeyError: If the specified name does not exist in the dataset.
        """
        pass

    @property
    @abstractmethod
    def spacing(self) -> tuple[int, int, int]:
        """Voxel spacing in physical units for (x, y, z) dimensions.

        Units depend on the dataset but are typically in nanometers or similar
        physical units.
        """
        pass

    @spacing.setter
    @abstractmethod
    def spacing(self, v: tuple[int, int, int]):
        """Set the voxel spacing in physical units for (x, y, z) dimensions."""
        pass

    @property
    @abstractmethod
    def tilt_angles(self) -> np.ndarray | None:
        """Array of tilt angles for tomographic tilt series.

        Tilt angles are typically in degrees for each projection in a tilt series.
        Returns None if this is not a tilt series dataset or if tilt angles have
        not been set.
        """
        pass

    @tilt_angles.setter
    @abstractmethod
    def tilt_angles(self, v: np.ndarray | None):
        """Set the tilt angles for tomographic tilt series.

        Provide None to clear tilt angles.
        """
        pass

    @property
    @abstractmethod
    def tilt_axis(self) -> int | None:
        """The axis index around which tilting occurs in a tomographic tilt series.

        - 0 = x-axis
        - 1 = y-axis
        - 2 = z-axis
        - None = not applicable or not set
        """
        pass

    @property
    @abstractmethod
    def scan_ids(self) -> np.ndarray | None:
        """Array of scan IDs associated with each projection in a tilt series.

        Returns None if scan IDs have not been set.
        """
        pass

    @scan_ids.setter
    @abstractmethod
    def scan_ids(self, v: np.ndarray | None):
        """Set the scan IDs for projections in a tilt series.

        Provide None to clear scan IDs.
        """
        pass

    @property
    @abstractmethod
    def dark(self) -> np.ndarray | None:
        """Dark field calibration data.

        Dark field images are captured with no illumination and represent the
        baseline signal level of the detector (electronic noise, thermal noise, etc.).
        Returns None if not available.
        """
        pass

    @property
    @abstractmethod
    def white(self) -> np.ndarray | None:
        """White field (flat field) calibration data.

        White field images are captured with uniform illumination and no sample,
        representing the detector response and illumination variations. Used for
        flat-field correction. Returns None if not available.
        """
        pass

    @property
    @abstractmethod
    def file_name(self) -> str | None:
        """The original filename this dataset was loaded from.

        Returns None if the dataset was not loaded from a file or if the
        filename is not available.
        """
        return self._data_source.file_name

    @property
    @abstractmethod
    def metadata(self) -> dict:
        """Dictionary containing arbitrary metadata associated with the dataset.

        Can include acquisition parameters, instrument settings, timestamps, etc.
        """
        return self._data_source.metadata

    @abstractmethod
    def empty_copy(self) -> 'Dataset':
        """Return a new Dataset of the same concrete type with the
        same metadata (spacing, tilt angles, dimensions, file_name,
        ...) but no scalar arrays. Useful as a starting point for
        building a derived dataset that shares geometry with this one.
        """
        pass

    def apply_to_each_scalar_array(
            self, fn: Callable[[np.ndarray], np.ndarray | None]
    ) -> 'Dataset':
        """Return a new Dataset with each scalar array replaced by
        ``fn(array)``. The original dataset is not modified.
        Active-scalar selection is preserved when possible.

        ``fn`` may either return a new numpy array or mutate the input
        in place and return it; either form works. Returning
        ``None`` excludes that array from the output — useful for
        filtering arrays out of the dataset alongside the per-array
        transformation.

        If the original active-scalar array is filtered out (``fn``
        returned ``None`` for it), the first remaining array becomes
        the new active scalar. If every array is filtered out, the
        result is an empty dataset.

        Useful when an operator's per-array logic should run uniformly
        across every scalar array in the dataset — saves writing the
        explicit ``for name in dataset.scalars_names: …`` loop.

        Example:

        .. code-block:: python

            class AddConstant(tomviz.nodes.TransformNode):
                def transform(self, inputs, constant=0.0):
                    ds = inputs["volume"]
                    return {
                        "volume": ds.apply_to_each_scalar_array(
                            lambda a: a + constant)
                    }
        """
        result = self.empty_copy()
        active = self.active_name
        # Snapshot names so iteration is stable if fn does anything
        # surprising to the source dataset.
        names = list(self.scalars_names)
        for name in names:
            new_arr = fn(self.scalars(name))
            if new_arr is None:
                continue
            result.set_scalars(name, new_arr)
        if active is not None and active in result.scalars_names:
            result.active_name = active
        elif result.scalars_names:
            # The original active was filtered out — fall back to the
            # first remaining scalar so the output's active_name
            # references an array that actually exists.
            result.active_name = result.scalars_names[0]
        return result
