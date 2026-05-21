# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

import json

import numpy as np

from tomviz.io import FileType, IOBase, Reader, Writer

import tomviz.internal_utils

from vtk import vtkImageData

try:
    import itk
    _HAS_ITK = True
except ImportError:
    _HAS_ITK = False

_IMAGE_COMMENTS_TAG = '0020|4000'
_SERIES_DESCRIPTION_TAG = '0008|103e'

_TOMVIZ_SERIES_DESCRIPTION = 'tomviz'


if _HAS_ITK:

    class DicomBase(IOBase):

        @staticmethod
        def file_type():
            return FileType('DICOM format', ['dcm'])

    class DicomReader(Reader, DicomBase):

        def read(self, path):
            gdcm_io = itk.GDCMImageIO.New()
            gdcm_io.SetLoadPrivateTags(True)
            itk_image = itk.imread(path, imageio=gdcm_io)
            data = itk.array_from_image(itk_image)

            if data.ndim == 2:
                data = data[np.newaxis, :, :]

            if data.ndim != 3:
                return vtkImageData()

            data = np.asfortranarray(np.transpose(data))

            image_data = vtkImageData()
            x, y, z = data.shape

            spacing = list(itk_image.GetSpacing())
            while len(spacing) < 3:
                spacing.append(1.0)
            origin = list(itk_image.GetOrigin())
            while len(origin) < 3:
                origin.append(0.0)

            image_data.SetOrigin(origin[0], origin[1], origin[2])
            image_data.SetSpacing(spacing[0], spacing[1], spacing[2])
            image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
            tomviz.internal_utils.set_array(image_data, data)

            md = gdcm_io.GetMetaDataDictionary()
            if md.HasKey(_IMAGE_COMMENTS_TAG):
                try:
                    meta = json.loads(md[_IMAGE_COMMENTS_TAG].strip())
                    if 'tilt_angles' in meta:
                        angles = np.array(meta['tilt_angles'],
                                          dtype=np.float64)
                        tomviz.internal_utils.set_tilt_angles(image_data,
                                                              angles)
                except (json.JSONDecodeError, ValueError):
                    pass

            return image_data

    class DicomWriter(Writer, DicomBase):

        def write(self, path, data_object):
            data = tomviz.internal_utils.get_array(data_object)
            spacing = data_object.GetSpacing()
            origin = data_object.GetOrigin()

            if data.dtype not in (np.uint8, np.uint16):
                print(f'DICOM only supports uint8/uint16 data. Converting '
                      f'from {data.dtype} to uint16.')
                dmin = data.min()
                dmax = data.max()
                if dmax > dmin:
                    data = ((data - dmin) / (dmax - dmin) * 65535).astype(
                        np.uint16)
                else:
                    data = np.zeros_like(data, dtype=np.uint16)

            data = np.ascontiguousarray(np.transpose(data))

            itk_image = itk.image_from_array(data)
            itk_image.SetSpacing([float(spacing[0]), float(spacing[1]),
                                  float(spacing[2])])
            itk_image.SetOrigin([float(origin[0]), float(origin[1]),
                                 float(origin[2])])

            md = itk_image.GetMetaDataDictionary()
            md[_SERIES_DESCRIPTION_TAG] = _TOMVIZ_SERIES_DESCRIPTION

            tomviz_meta = {}
            tilt_angles = tomviz.internal_utils.get_tilt_angles(data_object)
            if tilt_angles is not None:
                tomviz_meta['tilt_angles'] = list(
                    np.asarray(tilt_angles, dtype=np.float64))

            if tomviz_meta:
                md[_IMAGE_COMMENTS_TAG] = json.dumps(tomviz_meta)

            itk.imwrite(itk_image, path)
