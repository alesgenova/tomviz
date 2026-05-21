###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Schema-v2 state file loader. Mirrors PipelineStateIO::load on the C++
side closely enough that any state file produced by the new tomviz can
be reloaded into a pure-Python Pipeline.

Two file shapes are supported:
  - .tvsm / .json: plain JSON. Source nodes read external files at execute().
  - .tvh5: HDF5 container with `/tomviz_state` (JSON blob) plus voxel groups
    at `/data/<nodeId>/<portName>` referenced via `dataRef` entries on the
    serialized output ports. We pre-populate those ports and mark the
    owning nodes as Current so the executor skips their execute().
"""

import json
import logging
from pathlib import Path

import h5py
import numpy as np

from tomviz.external_dataset import Dataset
from tomviz.io_emd import ANGLE_UNITS, DIMS, Dim
from tomviz.pipeline.factory import NodeFactory, register_builtins
from tomviz.pipeline.node import NodeState, Pipeline, PortData


logger = logging.getLogger('tomviz.pipeline')


def load_state(state_file_path) -> Pipeline:
    """Read a schema-v2 .tvsm/.json or .tvh5 state file and build a
    Pipeline. The base directory is recorded on every source node so
    relative file paths resolve correctly."""
    register_builtins()

    state_file_path = Path(state_file_path)
    suffix = state_file_path.suffix.lower()

    if suffix == '.tvh5':
        state, raw_state_json = _read_tvh5_state(state_file_path)
    else:
        with open(state_file_path) as f:
            state = json.load(f)
        raw_state_json = state

    schema_version = state.get('schemaVersion')
    if schema_version != 2:
        raise ValueError(
            f'Unsupported schemaVersion {schema_version!r}; '
            'this CLI only loads schema-v2 state files.')

    pipeline_obj = state.get('pipeline')
    if not pipeline_obj:
        raise ValueError("State file is missing the 'pipeline' section")

    pipeline = Pipeline()
    pipeline.state_dir = state_file_path.parent
    pipeline.state_path = state_file_path

    id_to_node = {}
    nodes_json_by_id = {}
    for entry in pipeline_obj.get('nodes', []):
        type_name = entry.get('type')
        node_id = entry.get('id', -1)
        if not type_name or node_id < 0:
            logger.warning('Skipping node with missing id/type: %r', entry)
            continue

        node = NodeFactory.create(type_name)
        if node is None:
            logger.warning("Unknown node type '%s' (id=%d) — skipping",
                           type_name, node_id)
            continue

        # Stamp the state dir before deserialize so source nodes can
        # resolve relative file paths.
        node._state_dir = pipeline.state_dir

        if not node.deserialize(entry):
            logger.warning("Deserialize failed for node '%s' (id=%d)",
                           type_name, node_id)
            continue

        pipeline.add_node(node)
        pipeline.set_node_id(node, node_id)
        id_to_node[node_id] = node
        nodes_json_by_id[node_id] = entry

    if 'nextNodeId' in pipeline_obj:
        pipeline._next_node_id = max(pipeline._next_node_id,
                                     int(pipeline_obj['nextNodeId']))

    for entry in pipeline_obj.get('links', []):
        from_obj = entry.get('from', {})
        to_obj = entry.get('to', {})
        from_id = from_obj.get('node', -1)
        to_id = to_obj.get('node', -1)
        from_node = id_to_node.get(from_id)
        to_node = id_to_node.get(to_id)
        if from_node is None or to_node is None:
            logger.warning('Link references unknown node: %d -> %d',
                           from_id, to_id)
            continue
        from_port = from_node.output_port(from_obj.get('port'))
        to_port = to_node.input_port(to_obj.get('port'))
        if from_port is None or to_port is None:
            logger.warning('Link references unknown port: %s -> %s',
                           from_obj.get('port'), to_obj.get('port'))
            continue
        pipeline.create_link(from_port, to_port)

    # The saved `state` field on each node reflects the in-app session
    # at save time (e.g. every node Current after a successful run). For
    # the CLI we always start from a clean slate — overriding everything
    # back to New here ensures the executor actually runs each node.
    # The tvh5 dataRef path below re-marks individual nodes Current
    # after populating their output ports with the persisted voxels.
    for node in pipeline.nodes:
        node.state = NodeState.New

    if suffix == '.tvh5':
        _populate_tvh5_payloads(pipeline, raw_state_json, state_file_path,
                                id_to_node, nodes_json_by_id)

    return pipeline


def read_state_json(state_file_path) -> dict:
    """Return the raw schema-v2 state dict for either a ``.tvsm`` JSON
    file or the ``/tomviz_state`` blob inside a ``.tvh5`` HDF5 file —
    without building a Pipeline. Used by callers (e.g. the runner) that
    want to snapshot the saved state before mutating it."""
    state_file_path = Path(state_file_path)
    if state_file_path.suffix.lower() == '.tvh5':
        state, _ = _read_tvh5_state(state_file_path)
        return state
    with open(state_file_path) as f:
        return json.load(f)


def _read_tvh5_state(path: Path):
    """Return (state_dict, raw_pipeline_json). raw_pipeline_json is the
    pipeline section as parsed from `/tomviz_state` — we keep it around
    so dataRef entries on output ports can be resolved after the graph
    is built."""
    with h5py.File(path, 'r') as f:
        if 'tomviz_state' not in f:
            raise ValueError(
                f'{path}: not a tomviz .tvh5 file (no /tomviz_state)')
        bytes_data = f['tomviz_state'][()]
    if isinstance(bytes_data, np.ndarray):
        bytes_data = bytes_data.tobytes()
    if isinstance(bytes_data, (bytes, bytearray)):
        text = bytes_data.decode('utf-8')
    else:
        text = str(bytes_data)
    state = json.loads(text)
    return state, state


def _populate_tvh5_payloads(pipeline, raw_state, tvh5_path: Path,
                            id_to_node, nodes_json_by_id):
    """For every output port that carries a dataRef pointing into the
    .tvh5 file, read the payload group and stash it on the port. Volume
    ports get a Dataset (via :func:`_read_emd_group`); Table ports get a
    vtkTable (via :func:`_read_table_group`); Molecule ports a
    vtkMolecule (via :func:`_read_molecule_group`). The owning node is
    marked Current so the executor skips its execute() — payloads are
    already there."""
    with h5py.File(tvh5_path, 'r') as f:
        for node_id, node in id_to_node.items():
            entry = nodes_json_by_id.get(node_id, {})
            outputs = entry.get('outputPorts', {}) or {}
            populated_any = False
            for port_name, port_entry in outputs.items():
                data_ref = port_entry.get('dataRef')
                if not data_ref or data_ref.get('container') != 'h5':
                    continue
                ref_path = data_ref.get('path', '')
                if not ref_path or ref_path not in f:
                    logger.warning('dataRef target missing in %s: %s',
                                   tvh5_path, ref_path)
                    continue
                port = node.output_port(port_name)
                if port is None:
                    continue
                try:
                    if port.port_type == 'Table':
                        payload = _read_table_group(f[ref_path])
                    elif port.port_type == 'Molecule':
                        payload = _read_molecule_group(f[ref_path])
                    else:
                        payload = _read_emd_group(f[ref_path])
                except Exception:
                    logger.exception('Failed to read dataRef %s', ref_path)
                    continue
                port.set_data(PortData(payload, port.port_type))
                populated_any = True
            if populated_any:
                node.state = NodeState.Current


def _read_emd_group(group) -> Dataset:
    """Read a per-port EMD node out of an open .tvh5. Mirrors C++
    ``EmdFormat::readNode`` — datasets sit *directly* under ``group``
    (``group/data``, ``group/dim1`` etc.), without the
    ``/data/tomography`` outer wrapper that a stand-alone .emd file
    has."""

    def bytes_to_str(x):
        if isinstance(x, (bytes, bytearray, np.bytes_)):
            return x.decode('utf-8')
        return x

    dims = []
    for d in DIMS:
        dims.append(Dim(d,
                        group[d][:],
                        bytes_to_str(group[d].attrs['name']),
                        bytes_to_str(group[d].attrs['units'])))

    data = group['data']
    name = data.attrs.get('name', 'ImageScalars')
    if isinstance(name, (np.ndarray, list, tuple)):
        name = name[0]
    if isinstance(name, (bytes, bytearray)):
        name = name.decode()

    arrays = [(name, data[:])]

    tomviz_scalars = group.get('tomviz_scalars')
    if isinstance(tomviz_scalars, h5py.Group):
        def is_hard_link(n):
            link = tomviz_scalars.get(n, getlink=True)
            return isinstance(link, h5py.HardLink)
        keys = list(filter(is_hard_link, tomviz_scalars.keys()))
        arrays += [(k, tomviz_scalars[k][:]) for k in keys]

    tilt_axis = None
    if (dims[0].name in ('angles', b'angles') or
            dims[0].units in ANGLE_UNITS):
        arrays = [(n, np.transpose(a, [2, 1, 0])) for (n, a) in arrays]
        # swap dims 0 and -1
        tmp = dims[0]
        dims[0] = Dim(dims[0].path, dims[-1].values,
                      dims[-1].name, dims[-1].units)
        dims[-1] = Dim(dims[-1].path, tmp.values, tmp.name, tmp.units)
        tilt_axis = 2

    arrays = [(n, np.asfortranarray(a)) for (n, a) in arrays]

    # /data is the first scalar; active is marked separately. Files
    # without the attribute fall back to the primary.
    primary_name, _ = arrays[0]
    active_attr = group.attrs.get('active_scalar_name')
    if isinstance(active_attr, (np.ndarray, list, tuple)):
        active_attr = active_attr[0]
    if isinstance(active_attr, (bytes, bytearray)):
        active_attr = active_attr.decode()
    arrays_dict = {n: a for (n, a) in arrays}
    active_name = active_attr if (active_attr in arrays_dict) else primary_name
    dataset = Dataset(arrays_dict, active_name)
    if dims[-1].name in ('angles', b'angles'):
        dataset.tilt_angles = dims[-1].values[:].astype(np.float64)
    if tilt_axis is not None:
        dataset.tilt_axis = tilt_axis
    dataset.spacing = [float(d.values[1] - d.values[0])
                       if len(d.values) > 1 else 1.0 for d in dims]
    dataset.dims = dims
    return dataset


# Mirrors VTK's vtkType.h. Used as the ``vtkDataType`` attribute on each
# column dataset to pick the matching vtkAbstractArray subclass on read.
_VTK_STRING = 13


def _read_molecule_group(group) -> 'vtk.vtkMolecule':
    """Reverse of
    :func:`tomviz.pipeline.state_writer._write_molecule_into`. Rebuilds
    a vtkMolecule from the ``atomicNumbers`` / ``atomPositions`` /
    ``bondAtoms`` / ``bondOrders`` datasets. Mirrors C++
    ``Tvh5Format::readMoleculePayload`` so the layout is identical
    across writers."""
    from vtk import vtkMolecule

    molecule = vtkMolecule()
    num_atoms = int(group.attrs.get('numAtoms', 0))
    num_bonds = int(group.attrs.get('numBonds', 0))

    if num_atoms > 0:
        if 'atomicNumbers' not in group or 'atomPositions' not in group:
            logger.warning('Molecule group missing atom datasets: %s',
                           group.name)
            return molecule
        atomic_numbers = np.asarray(group['atomicNumbers'][()])
        positions = np.asarray(group['atomPositions'][()]).reshape(-1, 3)
        for i in range(num_atoms):
            molecule.AppendAtom(int(atomic_numbers[i]),
                                float(positions[i, 0]),
                                float(positions[i, 1]),
                                float(positions[i, 2]))

    if num_bonds > 0:
        if 'bondAtoms' not in group:
            logger.warning('Molecule group missing bondAtoms: %s',
                           group.name)
            return molecule
        bond_atoms = np.asarray(group['bondAtoms'][()]).reshape(-1, 2)
        if 'bondOrders' in group:
            bond_orders = np.asarray(group['bondOrders'][()])
        else:
            bond_orders = np.ones(num_bonds, dtype=np.uint16)
        for i in range(num_bonds):
            molecule.AppendBond(int(bond_atoms[i, 0]),
                                int(bond_atoms[i, 1]),
                                int(bond_orders[i]))
    return molecule


def _read_table_group(group) -> 'vtk.vtkTable':
    """Reverse of :func:`tomviz.pipeline.state_writer._write_table_into`.
    Each ``c<i>`` sub-dataset rebuilds one column; ``vtkDataType`` picks
    the array subclass (``vtkStringArray`` for string columns,
    everything else routed through ``numpy_to_vtk`` with the original
    VTK array type so the dtype round-trips losslessly)."""
    from vtk import vtkStringArray, vtkTable
    from vtk.util.numpy_support import numpy_to_vtk

    def attr_str(value):
        if isinstance(value, (bytes, bytearray, np.bytes_)):
            return value.decode('utf-8')
        return str(value) if value is not None else ''

    table = vtkTable()
    num_columns = int(group.attrs.get('numColumns', 0))
    for i in range(num_columns):
        dataset_name = f'c{i}'
        if dataset_name not in group:
            logger.warning('Table column dataset missing: %s/%s',
                           group.name, dataset_name)
            continue
        ds = group[dataset_name]
        vtk_data_type = int(ds.attrs.get('vtkDataType', 0))
        name = attr_str(ds.attrs.get('name', ''))
        if vtk_data_type == _VTK_STRING:
            blob = ds[()]
            if isinstance(blob, np.ndarray):
                blob = blob.tobytes()
            text = blob.decode('utf-8') if isinstance(
                blob, (bytes, bytearray)) else str(blob)
            values = json.loads(text)
            column = vtkStringArray()
            column.SetNumberOfValues(len(values))
            for j, v in enumerate(values):
                column.SetValue(j, str(v))
        else:
            np_array = ds[()]
            # numpy_to_vtk copies the buffer and returns a vtkDataArray
            # whose subclass is dictated by ``array_type`` — so writing
            # a vtkIntArray and reading back with array_type=VTK_INT
            # yields another vtkIntArray.
            number_of_components = int(ds.attrs.get('numberOfComponents', 1))
            if np_array.ndim == 1 and number_of_components > 1:
                np_array = np_array.reshape(-1, number_of_components)
            column = numpy_to_vtk(num_array=np_array, deep=True,
                                  array_type=vtk_data_type)
        if name:
            column.SetName(name)
        table.AddColumn(column)
    return table
