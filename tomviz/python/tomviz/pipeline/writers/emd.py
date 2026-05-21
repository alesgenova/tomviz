###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""EMD writer for ImageData/Volume/TiltSeries port payloads."""

from pathlib import Path

from tomviz.io_emd import _write_emd


def write_emd(payload, target_path: Path) -> None:
    if not hasattr(payload, 'arrays'):
        raise TypeError(
            'EMD writer expected a tomviz Dataset, got '
            f'{type(payload).__name__}')
    _write_emd(target_path, payload)
