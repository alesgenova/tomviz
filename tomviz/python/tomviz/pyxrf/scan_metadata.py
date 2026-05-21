"""Read scan metadata from PyXRF HDF5 files for the dialog scan table."""
from __future__ import annotations

import glob
import os

import h5py


def _expand_scan_range(scan_range: str) -> list[int]:
    if not scan_range or not scan_range.strip():
        return []
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
    return sorted(ids)


def read_scan_metadata(working_directory: str,
                       scan_range: str = '') -> list[dict]:
    """Return a list of scan metadata dicts from HDF5 files in *working_directory*.

    Each dict has keys: scan_id, theta, status, filename.

    If *scan_range* is provided, any ID in the range without an HDF5 file
    gets a ``"fail"`` status entry.  The list is sorted by theta for
    successful scans, with failed scans appended at the end sorted by ID.
    """
    wd = working_directory

    # Build a map from scan_id -> file path for all matching HDF5 files
    files_by_id: dict[int, str] = {}
    for path in glob.glob(os.path.join(wd, 'scan2D_*.h5')):
        basename = os.path.basename(path)
        # Extract scan ID from filename like scan2D_12345.h5
        try:
            sid = int(basename.replace('scan2D_', '').replace('.h5', ''))
            files_by_id[sid] = path
        except ValueError:
            pass

    # Read metadata from HDF5 files
    found: dict[int, dict] = {}
    failed_files: dict[int, str] = {}
    for sid, path in files_by_id.items():
        try:
            with h5py.File(path, 'r') as f:
                md = f['xrfmap/scan_metadata']
                scan_id = int(md.attrs['scan_id'])
                theta = float(md.attrs['param_theta'])
                theta_units = md.attrs.get('param_theta_units', 'deg')
                if theta_units == 'mdeg':
                    theta /= 1000.0
                theta = round(theta, 3)
                status = str(md.attrs.get('scan_exit_status', ''))
                found[scan_id] = {
                    'scan_id': scan_id,
                    'theta': theta,
                    'status': status if status else 'success',
                    'filename': os.path.basename(path),
                }
        except Exception:
            failed_files[sid] = os.path.basename(path)

    expected_ids = _expand_scan_range(scan_range) if scan_range else []

    if expected_ids:
        results = []
        for sid in expected_ids:
            if sid in found:
                results.append(found[sid])
            elif sid in failed_files:
                results.append({
                    'scan_id': sid,
                    'theta': 0.0,
                    'status': 'fail',
                    'filename': failed_files[sid],
                })
            else:
                results.append({
                    'scan_id': sid,
                    'theta': 0.0,
                    'status': 'missing',
                    'filename': '',
                })
    else:
        results = list(found.values())
        for sid, fname in failed_files.items():
            results.append({
                'scan_id': sid,
                'theta': 0.0,
                'status': 'fail',
                'filename': fname,
            })

    results.sort(key=lambda r: r['scan_id'])
    return results
