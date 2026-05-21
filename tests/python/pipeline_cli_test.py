###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""End-to-end CLI test: build a tiny EMD on disk, hand-craft a schema-v2
state file with a Reader → ConvertToFloat → VolumeSink graph, run the
CLI via Click's test runner, and verify the output ends up as an EMD
under the chosen output directory."""

import json
from pathlib import Path

import h5py
import numpy as np
from click.testing import CliRunner

from tomviz.cli import main
from tomviz.external_dataset import Dataset
from tomviz.io_emd import _write_emd, load_dataset


def _write_simple_emd(path: Path, arr: np.ndarray):
    ds = Dataset({'ImageScalars': np.asfortranarray(arr)}, 'ImageScalars')
    ds.spacing = (1.0, 1.0, 1.0)
    _write_emd(path, ds)


def test_cli_executes_simple_state(tmp_path):
    # Step 1: synthetic input EMD
    arr = (np.arange(24, dtype=np.uint8) + 1).reshape((2, 3, 4))
    input_emd = tmp_path / 'input.emd'
    _write_simple_emd(input_emd, arr)

    # Step 2: schema-v2 state describing Reader → ConvertToFloat → Sink.
    # The sink must be ignored by the CLI; ConvertToFloat is the only
    # data leaf because its consumer is the (sink-typed) node 3.
    state = {
        'schemaVersion': 2,
        'pipeline': {
            'nextNodeId': 4,
            'nodes': [
                {'id': 1, 'type': 'source.reader',
                 'label': 'Reader',
                 'fileNames': [str(input_emd)]},
                {'id': 2, 'type': 'transform.convertToFloat',
                 'label': 'CvtFloat'},
                {'id': 3, 'type': 'sink.volume',
                 'label': 'Volume',
                 'inputPorts': {'volume': {'type': ['ImageData']}}},
            ],
            'links': [
                {'from': {'node': 1, 'port': 'volume'},
                 'to':   {'node': 2, 'port': 'volume'}},
                {'from': {'node': 2, 'port': 'output'},
                 'to':   {'node': 3, 'port': 'volume'}},
            ],
        },
    }
    state_path = tmp_path / 'state.tvsm'
    state_path.write_text(json.dumps(state))

    out_dir = tmp_path / 'out'
    runner = CliRunner()
    result = runner.invoke(main, [
        '-s', str(state_path),
        '-o', str(out_dir),
        '-p', 'tqdm',
    ])
    assert result.exit_code == 0, result.output
    # Single run → outputs land directly in <out>/.
    written = list(out_dir.glob('*.emd'))
    # ConvertToFloat is the single data leaf (the sink consumer doesn't
    # disqualify it); its only output port is "output".
    assert len(written) == 1
    assert written[0].name.startswith('2_')
    assert written[0].name.endswith('__output.emd')
    # And the contents really are float32.
    out_ds = load_dataset(written[0])
    assert out_ds.active_scalars.dtype == np.float32


def test_cli_writes_table_leaf_as_csv(tmp_path):
    """A graph whose only data leaf produces a vtkTable should land as
    a .csv file in the output directory."""
    import csv as _csv

    # Inline operator script that produces a tiny vtkTable. We bypass
    # tomviz.utils.make_spreadsheet (which gates on require_internal_mode)
    # and build the table directly from the Python operator.
    script = '''
import vtk

def transform(dataset):
    t = vtk.vtkTable()
    radius = vtk.vtkFloatArray()
    radius.SetName("radius")
    radius.SetNumberOfTuples(2)
    radius.SetValue(0, 1.0)
    radius.SetValue(1, 2.5)
    t.AddColumn(radius)
    return {"stats": t}
'''
    description = json.dumps({
        'name': 'TableEmitter',
        'label': 'Table Emitter',
        'apply_to_each_array': False,
        'results': [{'name': 'stats', 'type': 'table'}],
    })

    arr = (np.arange(24, dtype=np.uint8) + 1).reshape((2, 3, 4))
    input_emd = tmp_path / 'input.emd'
    _write_simple_emd(input_emd, arr)

    state = {
        'schemaVersion': 2,
        'pipeline': {
            'nextNodeId': 3,
            'nodes': [
                {'id': 1, 'type': 'source.reader',
                 'label': 'Reader', 'fileNames': [str(input_emd)]},
                {'id': 2, 'type': 'transform.legacyPython',
                 'label': 'TableOp',
                 'description': description,
                 'script': script},
            ],
            'links': [
                {'from': {'node': 1, 'port': 'volume'},
                 'to':   {'node': 2, 'port': 'volume'}},
            ],
        },
    }
    state_path = tmp_path / 'state.tvsm'
    state_path.write_text(json.dumps(state))

    out_dir = tmp_path / 'out'
    runner = CliRunner()
    result = runner.invoke(main, ['-s', str(state_path),
                                  '-o', str(out_dir)])
    assert result.exit_code == 0, result.output

    # Two outputs: the operator's primary port (a Dataset) as EMD and
    # its declared "stats" table as CSV. We only assert on the CSV.
    csv_files = list(out_dir.glob('*.csv'))
    assert len(csv_files) == 1
    with open(csv_files[0], newline='') as f:
        rows = list(_csv.reader(f))
    assert rows[0] == ['radius']
    assert rows[1] == ['1.0']
    assert rows[2] == ['2.5']


def test_cli_rejects_legacy_state(tmp_path):
    legacy = {'dataSources': [{'reader': {'fileNames': []},
                               'operators': []}]}
    state_path = tmp_path / 'state.tvsm'
    state_path.write_text(json.dumps(legacy))

    out_dir = tmp_path / 'out'
    runner = CliRunner()
    result = runner.invoke(main, [
        '-s', str(state_path),
        '-o', str(out_dir),
    ])
    assert result.exit_code != 0
