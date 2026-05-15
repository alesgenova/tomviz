# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

# The MRC header dtype and mode tables below are adapted from the mrcfile
# package (https://github.com/ccpem/mrcfile), which is distributed under the
# BSD-3-Clause license. Copyright (c) 2016, Science and Technology Facilities
# Council.

import numpy as np

from tomviz.io import FileType, IOBase, Writer

import tomviz.internal_utils

HEADER_DTYPE = np.dtype([
    ('nx', 'i4'),
    ('ny', 'i4'),
    ('nz', 'i4'),
    ('mode', 'i4'),
    ('nxstart', 'i4'),
    ('nystart', 'i4'),
    ('nzstart', 'i4'),
    ('mx', 'i4'),
    ('my', 'i4'),
    ('mz', 'i4'),
    ('cella', [('x', 'f4'), ('y', 'f4'), ('z', 'f4')]),
    ('cellb', [('alpha', 'f4'), ('beta', 'f4'), ('gamma', 'f4')]),
    ('mapc', 'i4'),
    ('mapr', 'i4'),
    ('maps', 'i4'),
    ('dmin', 'f4'),
    ('dmax', 'f4'),
    ('dmean', 'f4'),
    ('ispg', 'i4'),
    ('nsymbt', 'i4'),
    ('extra1', 'V8'),
    ('exttyp', 'S4'),
    ('nversion', 'i4'),
    ('extra2', 'V84'),
    ('origin', [('x', 'f4'), ('y', 'f4'), ('z', 'f4')]),
    ('map', 'S4'),
    ('machst', 'u1', 4),
    ('rms', 'f4'),
    ('nlabl', 'i4'),
    ('label', 'S80', 10),
])

_DTYPE_TO_MODE = {
    'i1': 0,
    'i2': 1,
    'f4': 2,
    'u2': 6,
}


class MrcBase(IOBase):

    @staticmethod
    def file_type():
        return FileType('MRC format', ['mrc', 'mrc2'])


class MrcWriter(Writer, MrcBase):

    def write(self, path, data_object):
        data = tomviz.internal_utils.get_array(data_object)
        data = np.ascontiguousarray(np.transpose(data))

        kind_and_size = data.dtype.kind + str(data.dtype.itemsize)
        if kind_and_size not in _DTYPE_TO_MODE:
            data = data.astype(np.float32)
            kind_and_size = 'f4'

        nz, ny, nx = data.shape
        spacing = data_object.GetSpacing()

        header = np.zeros(1, dtype=HEADER_DTYPE)
        header['nx'] = nx
        header['ny'] = ny
        header['nz'] = nz
        header['mode'] = _DTYPE_TO_MODE[kind_and_size]
        header['mx'] = nx
        header['my'] = ny
        header['mz'] = nz
        header['cella']['x'] = nx * spacing[0]
        header['cella']['y'] = ny * spacing[1]
        header['cella']['z'] = nz * spacing[2]
        header['cellb']['alpha'] = 90.0
        header['cellb']['beta'] = 90.0
        header['cellb']['gamma'] = 90.0
        header['mapc'] = 1
        header['mapr'] = 2
        header['maps'] = 3
        header['dmin'] = data.min()
        header['dmax'] = data.max()
        header['dmean'] = data.mean()
        header['nversion'] = 20140
        header['map'] = b'MAP '
        header['machst'] = [0x44, 0x44, 0, 0]
        header['rms'] = data.std()

        with open(path, 'wb') as f:
            f.write(header.tobytes())
            f.write(data.tobytes())
