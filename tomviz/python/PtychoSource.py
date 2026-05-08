from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np
from numpy.typing import NDArray
from scipy.ndimage.interpolation import rotate
from scipy.optimize import leastsq
from tqdm import tqdm

import tomviz.nodes

from tomviz.ptycho.ptycho import find_ptycho_file

if TYPE_CHECKING:
    from collections.abc import Callable

    from tomviz.dataset import Dataset


def _fit_func(p0: float, px1: float,
              py1: float) -> Callable[[NDArray, NDArray], NDArray]:
    return lambda x, y: p0 + x * px1 + py1 * y


def _remove_background(im: NDArray[np.floating]) -> NDArray[np.floating]:
    params = [0, 0, 0]

    def err_func(p: list[float]) -> NDArray:
        return np.ravel(_fit_func(*p)(*np.indices(im.shape)) - im)

    p, success = leastsq(err_func, params)
    return im - _fit_func(*p)(*np.indices(im.shape))


def _attempt_to_read_pixel_sizes(
    sid: int, version: str, ptycho_dir: str | Path,
) -> tuple[float, float] | None:
    from tomviz.ptycho.ptycho import (
        locate_ptycho_hyan_file,
        fetch_pixel_sizes_from_ptycho_hyan_file,
    )

    path = locate_ptycho_hyan_file(sid, version, ptycho_dir)
    if path is None:
        print(
            f'Failed to locate config file for {sid} {version}\n'
            f'Pixel sizes will not be read',
            file=sys.stderr,
        )
        return None

    result = fetch_pixel_sizes_from_ptycho_hyan_file(path)
    if result is None:
        print(
            'Failed to obtain pixel sizes. Pixel sizes '
            'will not be applied to datasets.',
            file=sys.stderr,
        )
        return None

    print('Pixel sizes identified as:', result[0], result[1])
    return result[0], result[1]


def _stack_ptycho_data(
    version_list: list[str],
    sid_list: list[int],
    angle_list: list[float],
    ptycho_dir: str,
    rotate_datasets: bool = True,
) -> dict[str, object]:
    angle_list, sid_list, version_list = (
        zip(*sorted(zip(angle_list, sid_list, version_list)))
    )

    filespty_obj: list[Path] = []
    filespty_prb: list[Path | None] = []
    currentsidlist: list[tuple[int, int, float, str]] = []

    if len(version_list) == 1:
        version_list = np.repeat(version_list, len(sid_list))

    ptycho_dir_path = Path(ptycho_dir)

    pixel_size_x: float | None = None
    pixel_size_y: float | None = None
    for i, sid in tqdm(enumerate(sid_list[0: len(version_list)]),
                       desc="Loading Ptycho"):
        sid = int(sid)
        version = version_list[i]
        f_path = find_ptycho_file(sid, version, 'object', ptycho_dir_path)
        g_path = find_ptycho_file(sid, version, 'probe', ptycho_dir_path)
        if f_path is not None:
            filespty_obj.append(f_path)
            currentsidlist.append((i, sid, angle_list[i], version))
            filespty_prb.append(g_path)
            if i == 0:
                print('Attempting to read pixel sizes from the '
                      f'first scan ID: {sid}')
                result = _attempt_to_read_pixel_sizes(
                    sid, version, ptycho_dir_path)
                if result is not None:
                    pixel_size_x, pixel_size_y = result
        else:
            print(f"didn't find: {sid}")

    has_pixel_sizes = pixel_size_x is not None
    print(f"found: {len(filespty_obj)}")

    tempPtyobj: list[NDArray] = []
    tempPtyprb: list[NDArray] = []
    tempPtyamp: list[NDArray] = []
    for i in tqdm(range(len(filespty_obj)), desc="processing ptycho"):
        obj: NDArray = np.load(filespty_obj[i])
        prb: NDArray = np.load(filespty_prb[i])

        if obj.ndim == 3:
            obj = obj[0]

        if prb.ndim == 3:
            prb = prb[0]

        space = 15
        obj = np.fliplr(np.rot90(obj))
        prb = np.fliplr(np.rot90(prb))
        prb_sz = np.shape(prb)
        obj_sz = np.shape(obj)
        obj_c = obj[
            int(prb_sz[0] / 2) + space: obj_sz[0] - int(prb_sz[0] / 2) - space,
            int(prb_sz[1] / 2) + space: obj_sz[1] - int(prb_sz[1] / 2) - space,
        ]
        obj_c_arg = np.angle(obj_c)
        obj_c_amp = np.abs(obj_c)
        if True:
            obj_c_arg = _remove_background(obj_c_arg)
        objectoutput = obj_c_amp * np.exp((0 + 1j) * obj_c_arg)
        obj_c_arg = np.angle(objectoutput)
        obj_c_amp = np.abs(obj_c)
        tempPtyamp.append(obj_c_amp)
        tempPtyobj.append(obj_c_arg * -1)
        tempPtyprb.append(prb)

    has_probes = True
    try:
        probes = np.asarray(tempPtyprb)
        probes_phase = np.angle(probes)
        probes_amp = np.abs(probes)
    except Exception as e:
        has_probes = False
        msg = (
            f'Failed to stack probes with error message: {e}\n'
            'Skipping over probe data...'
        )
        print(msg, file=sys.stderr)

    shapeslist = [i.shape for i in tempPtyobj]
    shapeslist = np.asarray(shapeslist)
    lmax = shapeslist[:, 0].max()
    wmax = shapeslist[:, 1].max()
    ptychodatanew = np.zeros((len(tempPtyobj), int(lmax), int(wmax)))
    ampdatanew = np.zeros((len(tempPtyobj), int(lmax), int(wmax)))
    for n, i in tqdm(enumerate(tempPtyobj), desc="correcting shape"):
        lerr = np.abs(i.shape[0] - lmax)
        werr = np.abs(i.shape[1] - wmax)
        ti = np.pad(i, ((lerr // 2, lerr // 2), (werr // 2, werr // 2)))
        ta = np.pad(
            tempPtyamp[n], ((lerr // 2, lerr // 2), (werr // 2, werr // 2)),
        )
        ptychodatanew[n, :, :] = ti
        ampdatanew[n, :, :] = ta

    arrays: dict[str, NDArray] = {
        'Phase': ptychodatanew,
        'Amplitude': ampdatanew,
    }
    if has_probes:
        arrays = {
            **arrays,
            'Probes Phase': probes_phase,
            'Probes Amplitude': probes_amp,
        }

    for key, array in arrays.items():
        if rotate_datasets:
            array = rotate(array, -90.0, axes=(1, 2))

        array = array.swapaxes(0, 2)
        arrays[key] = array

    return {
        'arrays': arrays,
        'tilt_angles': np.array([x[2] for x in currentsidlist]),
        'scan_ids': np.array([x[1] for x in currentsidlist], dtype=np.int32),
        'has_probes': has_probes,
        'has_pixel_sizes': has_pixel_sizes,
        'pixel_size_x': pixel_size_x,
        'pixel_size_y': pixel_size_y,
        'info_rows': currentsidlist,
    }


def _write_ptycho_info_file(
    output_path: str,
    info_rows: list[tuple[int, int, float, str]],
) -> None:
    currentsidlist_str: list[list[str]] = []
    for row in info_rows:
        this_row: list[str] = []
        for entry in row:
            if isinstance(entry, float):
                s = f'{entry:.3f}'
            else:
                s = str(entry)
            this_row.append(s)
        currentsidlist_str.append(this_row)

    col_delim = ' '
    headers = ['Angle', 'SID', 'Version']
    index_order = [2, 1, 3]
    col_width = 10
    with open(Path(output_path), 'w') as wf:
        header_str = col_delim.join([f'{x:>{col_width}}' for x in headers])
        header_str = '#' + header_str[1:]
        wf.write(header_str + '\n')
        for row_str_list in currentsidlist_str:
            row_str = col_delim.join([
                f'{row_str_list[idx]:>{col_width}}' for idx in index_order
            ])
            wf.write(row_str + '\n')


class PtychoSource(tomviz.nodes.SourceNode):

    def produce(self, ptycho_dir: str = '', output_info_file: str = '',
                rotate_datasets: bool = True, sid_list: str = '[]',
                version_list: str = '[]',
                angle_list: str = '[]') -> dict[str, Dataset] | None:
        parsed_sids: list[int] = json.loads(sid_list)
        parsed_versions: list[str] = json.loads(version_list)
        parsed_angles: list[float] = json.loads(angle_list)

        if not parsed_sids:
            print('No scan IDs provided')
            return None

        result = _stack_ptycho_data(
            parsed_versions, parsed_sids, parsed_angles,
            ptycho_dir, rotate_datasets,
        )

        arrays = result['arrays']

        if output_info_file:
            Path(output_info_file).parent.mkdir(parents=True, exist_ok=True)
            _write_ptycho_info_file(output_info_file, result['info_rows'])

        # Build the object dataset (Phase + Amplitude)
        object_ds = self.create_dataset()
        object_ds.set_scalars('Phase', arrays['Phase'])
        object_ds.set_scalars('Amplitude', arrays['Amplitude'])
        object_ds.tilt_angles = result['tilt_angles']
        object_ds.scan_ids = result['scan_ids']
        object_ds.tilt_axis = 2
        if result['has_pixel_sizes']:
            object_ds.spacing = (
                result['pixel_size_x'], result['pixel_size_y'], 1)

        outputs: dict[str, Dataset] = {'object': object_ds}

        # Build the probe dataset if probes are available
        if result['has_probes']:
            probe_ds = self.create_dataset()
            probe_ds.set_scalars('Probes Phase', arrays['Probes Phase'])
            probe_ds.set_scalars('Probes Amplitude', arrays['Probes Amplitude'])
            probe_ds.tilt_angles = result['tilt_angles']
            probe_ds.scan_ids = result['scan_ids']
            probe_ds.tilt_axis = 2
            outputs['probe'] = probe_ds

        return outputs
