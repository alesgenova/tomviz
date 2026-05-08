from pathlib import Path

import h5py

def ic_names(working_directory):
    # Find any HDF5 file in the working directory and grab the ic names
    wd = Path(working_directory)
    if not wd.is_dir():
        return []
    files = [x for x in wd.iterdir() if x.suffix == '.h5']
    if not files:
        return []

    first_file = files[0]

    with h5py.File(first_file, 'r') as rf:
        names = rf["xrfmap"]["scalers"]["name"]
        names = [x.decode() for x in names]
        return names
