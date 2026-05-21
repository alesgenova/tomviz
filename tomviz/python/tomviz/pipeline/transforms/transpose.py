###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""TransposeData — swaps the i and k axes of every scalar array. Mirrors
C++ TransposeDataTransform.

The C++ implementation has two variants ("C" and "Fortran" ordering)
that both perform the same i/k axis swap on a 3D volume — the labels
refer to memory layout, not the resulting array shape. We always do the
swap and let the dtype/layout handling fall to numpy."""

import copy

import numpy as np

from tomviz.pipeline.node import PortData, TransformNode


class TransposeDataTransform(TransformNode):
    type_name = 'transform.transposeData'

    def __init__(self):
        super().__init__()
        self.add_input('volume', 'ImageData')
        self.add_output('output', 'ImageData')
        self.label = 'Transpose Data'
        self._transpose_type = 'C'

    def deserialize(self, data: dict) -> bool:
        if not super().deserialize(data):
            return False
        if 'transposeType' in data:
            self._transpose_type = data['transposeType']
        return True

    def transform(self, inputs):
        primary = inputs.get('volume')
        if primary is None:
            return {}
        dataset = copy.deepcopy(primary.payload)
        for name in list(dataset.scalars_names):
            arr = dataset.scalars(name)
            if arr.ndim >= 3:
                # i/k swap; preserve the (k or C) memory layout choice
                swapped = np.transpose(arr, axes=(2, 1, 0))
                if self._transpose_type == 'Fortran':
                    swapped = np.asfortranarray(swapped)
                else:
                    swapped = np.ascontiguousarray(swapped)
                dataset.set_scalars(name, swapped)
        return {'output': PortData(dataset, primary.port_type)}
