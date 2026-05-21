from __future__ import annotations

import json
import os
import subprocess
import tempfile
from collections.abc import Callable
from pathlib import Path
from typing import TYPE_CHECKING

import h5py
import numpy as np
from numpy.typing import NDArray
from scipy.ndimage.interpolation import rotate

import tomviz.nodes

if TYPE_CHECKING:
    from tomviz.dataset import Dataset


def _expand_scan_range(scan_range: str,
                       skip_ids: list[int] | None = None) -> list[int]:
    if not scan_range or not scan_range.strip():
        return []
    skip_set = set(skip_ids) if skip_ids else set()
    ids: set[int] = set()
    for part in scan_range.split(','):
        part = part.strip()
        if not part:
            continue
        if ':' in part:
            pieces = part.split(':')
            start = int(pieces[0])
            stop = int(pieces[1])
            stride = int(pieces[2]) if len(pieces) > 2 else 1
            ids.update(range(start, stop + 1, stride))
        else:
            ids.add(int(part))
    return sorted(ids - skip_set)


def _run_command(args: list[str]) -> None:
    env = os.environ.copy()
    env['PYTHONUNBUFFERED'] = '1'
    print(f'Running: {" ".join(str(a) for a in args)}', flush=True)
    proc = subprocess.Popen(
        args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1, env=env,
    )
    assert proc.stdout is not None
    for line in proc.stdout:
        print(line, end='', flush=True)
    proc.wait()
    if proc.returncode != 0:
        raise RuntimeError(
            f'Command failed with exit code {proc.returncode}: '
            f'{" ".join(str(a) for a in args)}'
        )


def _run_make_hdf5(command: str, working_directory: Path,
                   scan_range: str, skip_scan_ids: list[int],
                   force: bool) -> None:
    args = [
        command, 'make-hdf5', str(working_directory),
        '--range', scan_range,
    ]
    if skip_scan_ids:
        args += ['--skip', json.dumps(skip_scan_ids)]
    if force:
        args.append('--force')
    _run_command(args)


def _run_process_projections(command: str, working_directory: Path,
                             scan_range: str, skip_scan_ids: list[int],
                             parameters_file: str, ic_name: str,
                             output_directory: Path,
                             skip_processed: bool,
                             csv_output: str) -> None:
    args = [
        command, 'process-projections', str(working_directory),
        '--range', scan_range,
        '-p', str(parameters_file),
        '-i', ic_name,
        '-o', str(output_directory),
    ]
    if skip_scan_ids:
        args += ['--skip', json.dumps(skip_scan_ids)]
    if skip_processed:
        args.append('-s')
    if csv_output:
        args += ['--csv-output', csv_output]
    _run_command(args)


def _read_hdf5_metadata(
    working_directory: Path,
    scan_ids: list[int],
) -> tuple[tuple[float, float] | None, list[int], NDArray[np.floating] | None]:
    """Read pixel sizes, valid scan IDs, and theta from HDF5 files."""
    pixel_sizes: tuple[float, float] | None = None
    valid_ids: list[int] = []
    thetas: list[float] = []

    for sid in scan_ids:
        path = working_directory / f'scan2D_{sid}.h5'
        if not path.exists():
            continue
        try:
            with h5py.File(path, 'r') as f:
                md = f['xrfmap/scan_metadata']
                theta = float(md.attrs['param_theta'])
                theta_units = md.attrs.get('param_theta_units', 'deg')
                if theta_units == 'mdeg':
                    theta /= 1000.0
                valid_ids.append(sid)
                thetas.append(theta)

                if pixel_sizes is None:
                    param_input = md.attrs['param_input']
                    x_start, x_stop, num_x = param_input[0], param_input[1], param_input[2]
                    y_start, y_stop, num_y = param_input[3], param_input[4], param_input[5]
                    if num_x > 0 and num_y > 0:
                        px = round((x_stop - x_start) / num_x * 1e3, 8)
                        py = round((y_stop - y_start) / num_y * 1e3, 8)
                        pixel_sizes = (px, py)
                        print(f'Pixel sizes from HDF5: {px} {py}')
        except Exception as exc:
            print(f'Warning: failed to read metadata from {path.name}: {exc}')

    theta_array = np.array(thetas, dtype=np.float64) if thetas else None
    return pixel_sizes, valid_ids, theta_array


def _read_tomo_h5(tomo_file: Path, rotate_datasets: bool,
                  pixel_sizes: tuple[float, float] | None,
                  scan_ids: list[int],
                  theta: NDArray[np.floating] | None,
                  create_dataset: Callable[[], Dataset]) -> Dataset:
    with h5py.File(tomo_file, 'r') as f:
        element_names: list[str] = [
            x.decode() if isinstance(x, bytes) else x
            for x in f['reconstruction/fitting/elements'][()]
        ]
        data: NDArray[np.floating] = f['reconstruction/fitting/data'][()]
        if theta is None:
            theta = f['exchange/theta'][()]

    ds = create_dataset()

    for i, name in enumerate(element_names):
        element_data = data[:, i, :, :]
        if rotate_datasets:
            element_data = rotate(element_data, -90.0, axes=(1, 2))
        element_data = element_data.swapaxes(0, 2)
        ds.set_scalars(name, element_data)

    ds.tilt_angles = theta
    ds.tilt_axis = 2

    if pixel_sizes is not None:
        ds.spacing = (pixel_sizes[0], pixel_sizes[1], 1)

    if scan_ids:
        ds.scan_ids = np.array(scan_ids, dtype=np.int32)

    return ds


class PyXRFSource(tomviz.nodes.SourceNode):

    def produce(self, pyxrf_utils_command: str = 'pyxrf-utils',
                working_directory: str = '',
                scan_range: str = '',
                skip_scan_ids: str = '[]',
                skip_downloads: bool = False,
                redownload_successful: bool = False,
                parameters_file: str = '',
                ic_name: str = 'sclr1_ch4',
                skip_processed: bool = True,
                rotate_datasets: bool = True,
                csv_output: str = '',
                ui_state: str = '{}') -> dict[str, Dataset]:
        working_dir = Path(working_directory)
        skip_ids: list[int] = json.loads(skip_scan_ids) if skip_scan_ids else []

        if scan_range and not skip_downloads:
            _run_make_hdf5(
                pyxrf_utils_command, working_dir,
                scan_range, skip_ids, redownload_successful,
            )

        if scan_range:
            with tempfile.TemporaryDirectory() as tmp_dir:
                output_dir = Path(tmp_dir)
                _run_process_projections(
                    pyxrf_utils_command, working_dir,
                    scan_range, skip_ids,
                    parameters_file, ic_name,
                    output_dir, skip_processed, csv_output,
                )
                return self._read_results(
                    output_dir, working_dir, scan_range,
                    skip_ids, rotate_datasets,
                )

        # No scan_range: expect tomo.h5 already in working_directory
        return self._read_results(
            working_dir, working_dir, '',
            skip_ids, rotate_datasets,
        )

    def _read_results(
        self, output_dir: Path, working_dir: Path,
        scan_range: str, skip_ids: list[int],
        rotate_datasets: bool,
    ) -> dict[str, Dataset]:
        tomo_file = output_dir / 'tomo.h5'
        if not tomo_file.exists():
            raise RuntimeError(
                f'tomo.h5 not found at {tomo_file} after processing'
            )

        all_ids = _expand_scan_range(scan_range, skip_ids) if scan_range else []
        pixel_sizes, valid_ids, theta = _read_hdf5_metadata(working_dir, all_ids)

        return {
            'elements': _read_tomo_h5(
                tomo_file, rotate_datasets, pixel_sizes,
                valid_ids, theta, self.create_dataset,
            ),
        }
