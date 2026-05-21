import math
from pathlib import Path
import sys

import numpy as np

PathLike = Path | str


def gather_ptycho_info(ptycho_dir: PathLike) -> dict:
    ptycho_dir = Path(ptycho_dir)
    if not ptycho_dir.is_dir():
        # It either doesn't exist or it's not a directory
        return {
            'sid_list': [],
            'version_list': [],
            'angle_list': [],
            'error_list': [],
        }

    sid_list = sorted([int(x.name[1:]) for x in ptycho_dir.iterdir()
                      if x.is_dir() and x.name.startswith('S')])

    sid_dirs = [ptycho_dir / f'S{sid}' for sid in sid_list]

    version_dict = {}
    for sid, d in zip(sid_list, sid_dirs):
        versions = []
        for subdir in d.iterdir():
            if subdir.is_dir():
                versions.append(subdir.name)

        if not versions:
            # There will be an error, but at least put in 't1'
            versions.append('t1')

        version_dict[sid] = sorted(versions)

    # Create angles and error lists for each SID and version
    angle_dict = {}
    error_dict = {}
    for sid in sid_list:
        these_angles = []
        these_errors = []
        for version in version_dict[sid]:
            these_angles.append(load_angle_from_sid(sid, version,
                                                    ptycho_dir))
            these_errors.append(validate_sid(sid, version, ptycho_dir))

        angle_dict[sid] = these_angles
        error_dict[sid] = these_errors

    version_list = [version_dict[sid] for sid in sid_list]
    angle_list = [angle_dict[sid] for sid in sid_list]
    error_list = [error_dict[sid] for sid in sid_list]
    return {
        'sid_list': sid_list,
        'version_list': version_list,
        'angle_list': angle_list,
        'error_list': error_list,
    }


def validate_sid(sid: int, version: str, ptycho_dir: PathLike) -> str:
    # Validate the sid and version, that it contains the data and angles.
    # If it is valid, the returned string will be empty. Otherwise, the
    # returned string will contain the error message as to what is not
    # valid.
    f_path = find_ptycho_file(sid, version, 'object', ptycho_dir)
    if f_path is None:
        return 'Ptycho data missing'

    g_path = find_ptycho_file(sid, version, 'probe', ptycho_dir)
    if g_path is None:
        return 'Probe data missing'

    angle = load_angle_from_sid(sid, version, ptycho_dir)
    if np.isnan(angle):
        return 'Angle not found'

    return ''


def find_ptycho_file(sid: int, version: str, type_str: str,
                     ptycho_dir: PathLike) -> Path | None:
    # type_str is `ptycho` or `probe`
    ptycho_dir = Path(ptycho_dir)
    dir_path = ptycho_dir / f'S{sid}/{version}/recon_data'
    base_str = f'recon_{sid}_{version}_{type_str}'
    # Prefer `_ave.npy` if available, then `.npy`
    suffix_to_try = [
        '_ave.npy',
        '.npy',
    ]
    for suffix in suffix_to_try:
        path = dir_path / f'{base_str}{suffix}'
        if path.exists():
            return path

    # If those didn't exist, just try to grab anything that
    # matches `{base_str}*.npy`
    paths = list(dir_path.glob(f'{base_str}*.npy'))
    if paths:
        return paths[0].resolve()

    # Didn't find any matches
    return None


def load_angle_from_sid(sid: int, version: str,
                        ptycho_dir: PathLike) -> float:
    path = locate_ptycho_hyan_file(sid, version, ptycho_dir)
    if path is None:
        return math.nan

    return fetch_angle_from_ptycho_hyan_file(path)


def locate_ptycho_hyan_file(sid: int, version: str,
                            ptycho_dir: PathLike) -> str | None:
    recon_data_dir = Path(ptycho_dir) / f'S{sid}' / version / 'recon_data'
    matches = list(recon_data_dir.glob(f'{sid}_{version}*'))

    for match in matches:
        try:
            with open(match.resolve(), 'r') as rf:
                if 'angle =' in rf.read():
                    return match.resolve()
        except Exception:
            # Move on to the next one
            continue

    return None


def get_use_and_versions_from_csv(csv_path: str) -> dict:
    data = np.genfromtxt(csv_path, delimiter=',', names=True, dtype=None)

    sids = []
    try:
        sids = data['Scan_ID'].tolist()
    except Exception:
        print('"Scan_ID" column not found in CSV file', file=sys.stderr)

    use = []
    try:
        use = data['Use'].tolist()
        # Convert to boolean
        use = [x in (1, '1', 'x') for x in use]
    except Exception:
        print('"Use" column not found in CSV file', file=sys.stderr)

    versions = []
    try:
        versions = data['Version'].tolist()
    except Exception:
        print('"Version" column not found in CSV file', file=sys.stderr)

    return {
        'sids': sids,
        'use': use,
        'versions': versions,
    }


def filter_sid_list(sid_list: list[int], filter_string: str) -> list[int]:
    if not filter_string.strip():
        # All SIDs are valid
        return list(sid_list)

    # Either a comma-delimited list or numpy slicing
    sid_strings = filter_string.split(',')
    valid_sids = []
    for this_str in sid_strings:
        if ':' in this_str:
            this_slice = slice(
                *(int(s) if s else None for s in this_str.split(':'))
            )
            # If there was no stop specified, go to the end of the sid_list
            if this_slice.stop is None:
                this_slice = slice(this_slice.start, max(sid_list) + 1,
                                   this_slice.step)
            else:
                # Unlike numpy, we want to be inclusive of the last number
                this_slice = slice(this_slice.start, this_slice.stop + 1,
                                   this_slice.step)

            valid_sids += np.r_[this_slice].tolist()
        else:
            valid_sids.append(int(this_str))

    return [sid for sid in sid_list if sid in valid_sids]


def fetch_angle_from_ptycho_hyan_file(filepath: PathLike) -> float | None:
    with open(filepath, 'r') as rf:
        for line in rf:
            line = line.lstrip()
            if line.startswith('angle = '):
                angle = float(line.split('=')[1].strip())
                return angle

    # Angle was not found
    return math.nan


def fetch_pixel_sizes_from_ptycho_hyan_file(
    filepath: PathLike,
) -> tuple[float, float] | None:
    print(f'Obtaining pixel sizes from config file: {filepath}')
    vars_required = [
        'lambda_nm', 'z_m', 'nx', 'ny', 'ccd_pixel_um'
    ]
    alternatives = {
        'nx': 'x_arr_size',
        'ny': 'y_arr_size',
    }
    vars_requested = vars_required + list(alternatives.values())
    results = {}
    try:
        with open(filepath, 'r') as rf:
            for line in rf:
                if '=' not in line:
                    continue

                lhs = line.split('=')[0].strip()
                if lhs in vars_requested:
                    value = float(line.split('=', 1)[1].strip())
                    results[lhs] = value
    except Exception as e:
        print('Failed to fetch pixel sizes with error:', e, file=sys.stderr)
        return None

    # Add alternatives if they are present
    for name in vars_required:
        if name not in results and name in alternatives:
            # Check the alt_name
            alt_name = alternatives[name]
            if alt_name in results:
                # Convert it
                results[name] = results.pop(alt_name)

    missing = [x for x in vars_required if x not in results]
    if missing:
        print(
            'Failed to fetch pixel sizes. Some required variables '
            f'were not found: {missing}'
        )
        return None

    # Now compute them. They can both use the same numerator
    numerator = (
        results['lambda_nm'] * results['z_m'] * 1e6 / results['ccd_pixel_um']
    )

    x_pixel_size = numerator / results['nx']
    y_pixel_size = numerator / results['ny']

    return x_pixel_size, y_pixel_size
