import collections.abc

import numpy as np

from tomviz.dataset import Dataset as AbstractDataset

ARRAY_TYPES = (collections.abc.Sequence, np.ndarray)


class Dataset(AbstractDataset):
    def __init__(self, arrays=None, active=None):
        # Holds the map of scalars name => array
        if arrays is None:
            arrays = {}
        self.arrays = arrays
        self.tilt_angles = None
        self.tilt_axis = None
        self.scan_ids = None
        # The currently active scalar
        self.active_name = active
        # If we weren't given the active array, set the first as the active
        # array.
        if active is None and len(arrays.keys()):
            self.active_name = next(iter(arrays.keys()))

        self._spacing = None

        # Dark and white backgrounds
        self.dark = None
        self.white = None

        # Filename and metadata
        self.file_name = None
        self.metadata = {}

    @property
    def active_scalars(self):
        return self.scalars(self.active_name)

    @active_scalars.setter
    def active_scalars(self, array):
        self.set_scalars(self.active_name, array)

    @property
    def num_scalars(self):
        return len(self.arrays)

    @property
    def scalars_names(self):
        return list(self.arrays.keys())

    def scalars(self, name=None):
        if name is None:
            name = self.active_name
        return self.arrays[name]

    def set_scalars(self, name, array):
        self.arrays[name] = array
        # Mirror internal_dataset.Dataset: the first scalar added to an
        # otherwise-empty dataset becomes the active one.
        if self._active_name is None:
            self._active_name = name

    @property
    def spacing(self):
        return self._spacing

    @spacing.setter
    def spacing(self, v):
        if not isinstance(v, ARRAY_TYPES):
            raise Exception('Spacing must be an iterable type')
        if not len(v) == 3:
            raise Exception('Length of spacing must be 3')

        self._spacing = v

    @property
    def active_name(self) -> str:
        return self._active_name

    @active_name.setter
    def active_name(self, v: str):
        self._active_name = v

    @property
    def tilt_axis(self) -> int | None:
        return self._tilt_axis

    @tilt_axis.setter
    def tilt_axis(self, v: int | None):
        self._tilt_axis = v

    @property
    def tilt_angles(self) -> np.ndarray | None:
        return self._tilt_angles

    @tilt_angles.setter
    def tilt_angles(self, v: np.ndarray | None):
        self._tilt_angles = v

    @property
    def file_name(self) -> str | None:
        return self._file_name

    @file_name.setter
    def file_name(self, v: str | None):
        self._file_name = v

    @property
    def metadata(self) -> dict:
        return self._metadata

    @metadata.setter
    def metadata(self, v: dict):
        self._metadata = v

    @property
    def dark(self) -> np.ndarray | None:
        return self._dark

    @dark.setter
    def dark(self, v: np.ndarray | None):
        self._dark = v

    @property
    def white(self) -> np.ndarray | None:
        return self._white

    @white.setter
    def white(self, v: np.ndarray | None):
        self._white = v

    @property
    def scan_ids(self) -> np.ndarray | None:
        return self._scan_ids

    @scan_ids.setter
    def scan_ids(self, v: np.ndarray | None):
        self._scan_ids = v

    def remove_scalars(self, name):
        if name not in self.arrays:
            raise KeyError(f"No scalar array named '{name}'")
        del self.arrays[name]
        if self.active_name == name and self.arrays:
            self.active_name = next(iter(self.arrays.keys()))

    def rename_active(self, new_name: str):
        self.arrays[new_name] = self.arrays.pop(self.active_name)
        # Keep the pointer in sync with the dict key.
        self._active_name = new_name

    def empty_copy(self) -> 'Dataset':
        # Shallow-copy the instance dict so metadata (spacing, tilt
        # angles, file_name, ...) carries over, then drop the arrays.
        # type(self) preserves the concrete subclass (Dataset vs
        # LegacyDataset) so the upgrade is sticky.
        new_ds = type(self).__new__(type(self))
        new_ds.__dict__.update(self.__dict__)
        new_ds.arrays = {}
        return new_ds


class LegacyDataset(Dataset):
    """Adds the v1 ``create_child_dataset`` API expected by
    ``tomviz.operators``-derived recon-style operators. The CLI's
    LegacyPythonTransform upgrades each input to a LegacyDataset
    before invoking a v1 operator; v2 nodes see the base
    :class:`Dataset` instead, so the legacy affordance never leaks
    into a v2 author's namespace."""

    @classmethod
    def from_dataset(cls, base: Dataset) -> 'LegacyDataset':
        """Build a LegacyDataset that shares state with @a base.
        Bypasses ``__init__`` and copies the instance dict, which is
        safe here because LegacyDataset adds no new instance state.
        Used by the CLI's LegacyPythonTransform to upgrade the
        deep-copied input on its way into a v1 operator."""
        legacy = cls.__new__(cls)
        legacy.__dict__.update(base.__dict__)
        return legacy

    def create_child_dataset(self):
        # Empty child — matches the in-app internal_dataset.LegacyDataset.
        # Inheriting the parent's scalars would leave the un-written
        # ones at the input shape while the active one takes the
        # output shape, breaking downstream readers.
        child = LegacyDataset({}, active=self.active_name)
        if self.spacing is not None:
            s = self.spacing
            if self.tilt_angles is not None:
                child.spacing = [s[0], s[1], s[0]]
            else:
                child.spacing = list(s)
        return child
