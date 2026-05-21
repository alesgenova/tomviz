###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""ReaderSourceNode — reads file(s) into a Dataset and exposes it on the
'volume' output port. Mirrors the C++ ReaderSourceNode."""

from pathlib import Path

from tomviz.io_emd import load_dataset
from tomviz.pipeline.node import PortData, SourceNode


class ReaderSourceNode(SourceNode):
    type_name = 'source.reader'

    def __init__(self):
        super().__init__()
        self.add_output('volume', 'ImageData', persistent=True)
        self.label = 'Reader'
        self.file_names: list[str] = []
        self.reader_options: dict = {}

    def deserialize(self, data: dict) -> bool:
        if not super().deserialize(data):
            return False
        self.file_names = list(data.get('fileNames', []))
        self.reader_options = dict(data.get('readerOptions', {}))
        return True

    def execute(self) -> bool:
        if not self.file_names:
            return False

        # Translate ParaView reader-descriptor options into kwargs that
        # tomviz.io_emd.load_dataset understands. Only the options that
        # actually affect the Python reader are honored.
        read_options = {}
        if 'subsampleSettings' in self.reader_options:
            read_options['subsampleSettings'] = (
                self.reader_options['subsampleSettings'])
        if self.reader_options.get('keepCOrdering'):
            read_options['keep_c_ordering'] = True

        # First file only — mirrors the C++ side: stack support belongs
        # to ParaView readers we don't replicate here.
        path = Path(self.file_names[0])
        if not path.is_absolute():
            # Resolve against the state file directory if possible.
            state_dir = getattr(self, '_state_dir', None)
            if state_dir is not None:
                path = (state_dir / path).resolve()

        dataset = load_dataset(path, read_options or None)

        port = self.output_port('volume')
        port_type = port.port_type if port is not None else 'ImageData'
        if dataset.tilt_angles is not None and port_type == 'ImageData':
            port_type = 'TiltSeries'
        if port is not None:
            port.port_type = port_type
            port.set_data(PortData(dataset, port_type))
        return True
