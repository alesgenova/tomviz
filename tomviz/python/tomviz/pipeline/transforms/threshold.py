###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Threshold — produces a binary float32 mask from the active scalar
array, with 1.0 inside [minValue, maxValue] and 0.0 outside. Mirrors
C++ ThresholdTransform.

Note: the C++ class does not currently serialize its min/max values
(no override of serialize/deserialize beyond TransformNode), so a
state-file-loaded ThresholdTransform always uses defaults until it is
edited interactively. We keep the same defaults here."""

import numpy as np

from tomviz.external_dataset import Dataset
from tomviz.pipeline.node import PortData, TransformNode


class ThresholdTransform(TransformNode):
    type_name = 'transform.threshold'

    def __init__(self):
        super().__init__()
        self.add_input('volume', 'ImageData')
        self.add_output('mask', 'LabelMap')
        self.label = 'Threshold'
        self._min_value: float = -1e30
        self._max_value: float = 0.0

    def deserialize(self, data: dict) -> bool:
        if not super().deserialize(data):
            return False
        if 'minValue' in data:
            self._min_value = float(data['minValue'])
        if 'maxValue' in data:
            self._max_value = float(data['maxValue'])
        return True

    def transform(self, inputs):
        primary = inputs.get('volume')
        if primary is None:
            return {}
        src = primary.payload
        active = src.active_scalars
        if active is None:
            return {}
        mask = np.where(
            (active >= self._min_value) & (active <= self._max_value),
            np.float32(1.0), np.float32(0.0))

        out = Dataset({'Mask': mask}, 'Mask')
        out.spacing = src.spacing
        out.metadata = dict(src.metadata) if src.metadata else {}
        return {'mask': PortData(out, 'LabelMap')}
