from __future__ import annotations

from pathlib import Path
import shutil

from xrf_tomo import process_proj, make_single_hdf

from .create_log_file import create_log_file


def process_projections(
    scan_ids: list[int],
    working_directory: str,
    parameters_file_name: str,
    ic_name: str,
    output_directory: str,
    skip_processed: bool = True,
    csv_output: str = '',
) -> None:
    """Process XRF projections: generate CSV, fit, and assemble tomo.h5.

    A temporary CSV log is always created inside *working_directory* for
    ``xrf_tomo``.  If *csv_output* is set, the CSV is also copied there.
    """
    wd = Path(working_directory)

    # Build a sid_selection string that create_log_file understands.
    # It accepts comma-separated individual IDs.
    sid_selection = ','.join(str(s) for s in scan_ids) if scan_ids else None

    log_file = str(wd / 'tomo_info.csv')
    create_log_file(
        log_file_name=log_file,
        working_directory=working_directory,
        sid_selection=sid_selection,
        skip_invalid=True,
    )

    process_proj(
        wd=working_directory,
        fn_param=parameters_file_name,
        fn_log=log_file,
        ic_name=ic_name,
        skip_processed=skip_processed,
    )

    Path(output_directory).mkdir(parents=True, exist_ok=True)
    make_single_hdf(
        fn='tomo.h5',
        fn_log=log_file,
        wd_src=working_directory,
        wd_dest=output_directory,
        ic_name=ic_name,
    )

    if csv_output:
        csv_out = Path(csv_output)
        csv_out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(log_file, csv_out)
        print(f'CSV log copied to {csv_out}', flush=True)
