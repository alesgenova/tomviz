from __future__ import annotations

from pathlib import Path

from pyxrf.api_dev import make_hdf
import pyxrf.model.load_data_from_db

if pyxrf.model.load_data_from_db.db is None:
    pyxrf.model.load_data_from_db.catalog_info.set_name('HXN')

    from hxntools.CompositeBroker import db
    pyxrf.model.load_data_from_db.db = db


def make_hdf5(
    scan_ids: list[int],
    working_directory: Path,
    force: bool = False,
) -> None:
    """Download HDF5 files for the given scan IDs.

    For each ID, checks whether ``scan2D_{id}.h5`` already exists.
    Skips the download if the file is present and *force* is False.
    Calls ``make_hdf`` with start==end so failures are silently skipped.
    """
    wd = Path(working_directory)
    wd.mkdir(parents=True, exist_ok=True)

    for sid in scan_ids:
        target = wd / f'scan2D_{sid}.h5'
        if target.exists() and not force:
            print(f'Scan {sid}: {target.name} already exists, skipping',
                  flush=True)
            continue

        print(f'Downloading scan {sid} ...', flush=True)
        try:
            make_hdf(
                sid, sid,
                wd=str(wd),
                file_overwrite_existing=force,
            )
        except Exception as exc:
            print(f'Warning: scan {sid} failed to download: {exc}',
                  flush=True)
