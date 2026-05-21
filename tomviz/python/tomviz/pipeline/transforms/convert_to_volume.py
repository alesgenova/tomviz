###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""ConvertToVolume — drops tilt-angle metadata, marking the dataset as a
plain volume rather than a tilt series. Mirrors C++ ConvertToVolumeTransform."""

import copy

from tomviz.pipeline.node import PortData, TransformNode


class ConvertToVolumeTransform(TransformNode):
    type_name = 'transform.convertToVolume'

    def __init__(self):
        super().__init__()
        self.add_input('volume', 'ImageData')
        self.add_output('output', 'Volume')
        self.label = 'Convert to Volume'
        self._output_type = 'Volume'

    def deserialize(self, data: dict) -> bool:
        if not super().deserialize(data):
            return False
        if 'outputType' in data:
            # C++ stores PortType as an int enum; we accept either int or
            # the string form for forward-compatibility.
            ot = data['outputType']
            if isinstance(ot, str):
                self._output_type = ot
        if 'outputLabel' in data:
            self.label = data['outputLabel']
        port = self.output_port('output')
        if port is not None:
            port.port_type = self._output_type
        return True

    def transform(self, inputs):
        primary = inputs.get('volume')
        if primary is None:
            return {}
        dataset = copy.deepcopy(primary.payload)
        dataset.tilt_angles = None
        dataset.tilt_axis = None
        return {'output': PortData(dataset, self._output_type)}
