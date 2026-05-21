###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Writers for tomviz state containers — the ``.tvh5`` format that
bundles a schema-v2 state JSON with per-port voxel data inside one
HDF5 file. Mirrors the C++ ``Tvh5Format::write`` so files produced
here can be loaded by the in-app pipeline as well as round-tripped
by :func:`tomviz.pipeline.state_io.load_state`."""

import copy
import json
import logging
from pathlib import Path

import h5py
import numpy as np

from tomviz.io_emd import _write_emd_node_into
from tomviz.pipeline.node import SinkNode


logger = logging.getLogger('tomviz')


# Mirrors VTK's vtkType.h. Used as the ``vtkDataType`` attribute on each
# column dataset so the reader can rebuild the matching vtkAbstractArray
# subclass.
VTK_STRING = 13


def _is_vtk_table(payload) -> bool:
    """Duck-type check: a vtkTable exposes column accessors. Avoids
    importing vtk at module load — the runner CLI doesn't need it for
    state files that contain only volumes."""
    return (hasattr(payload, 'GetNumberOfColumns')
            and hasattr(payload, 'GetNumberOfRows')
            and hasattr(payload, 'GetColumn'))


def _is_vtk_molecule(payload) -> bool:
    """Duck-type check for a vtkMolecule. Same rationale as
    :func:`_is_vtk_table`: avoid pulling vtk in until we actually need to
    serialize one."""
    return (hasattr(payload, 'GetNumberOfAtoms')
            and hasattr(payload, 'GetNumberOfBonds')
            and hasattr(payload, 'GetAtomicNumberArray'))


def _write_table_into(group: 'h5py.Group', table) -> None:
    """Serialize ``table`` (a vtkTable) under ``group`` using the same
    layout as C++ ``Tvh5Format::writeTablePayload``: each column becomes
    a sub-dataset ``c0``, ``c1``, … with attributes ``name``,
    ``vtkDataType`` and ``numberOfComponents``. Numeric columns are
    written as their native numpy dtype; string columns become a
    JSON-encoded int8 blob (matching the C++ writer, which can't use
    HDF5 variable-length strings through h5cpp's writeData<>)."""
    from vtk.util.numpy_support import vtk_to_numpy

    num_columns = int(table.GetNumberOfColumns())
    num_rows = int(table.GetNumberOfRows())
    group.attrs['type'] = 'table'
    group.attrs['numColumns'] = np.int64(num_columns)
    group.attrs['numRows'] = np.int64(num_rows)

    for i in range(num_columns):
        column = table.GetColumn(i)
        if column is None:
            continue
        name = column.GetName() or ''
        dataset_name = f'c{i}'
        if hasattr(column, 'GetValue') and not hasattr(column, 'GetTuple1'):
            # vtkStringArray: serialize as JSON.
            values = [column.GetValue(j)
                      for j in range(column.GetNumberOfValues())]
            blob = json.dumps(values).encode('utf-8')
            ds = group.create_dataset(
                dataset_name, data=np.frombuffer(blob, dtype='i1'))
            vtk_data_type = VTK_STRING
            number_of_components = 1
        else:
            # Any vtkDataArray subclass.
            np_array = vtk_to_numpy(column)
            ds = group.create_dataset(dataset_name, data=np_array)
            vtk_data_type = int(column.GetDataType())
            number_of_components = int(column.GetNumberOfComponents())
        ds.attrs['name'] = name
        ds.attrs['vtkDataType'] = np.int32(vtk_data_type)
        ds.attrs['numberOfComponents'] = np.int32(number_of_components)


def _write_molecule_into(group: 'h5py.Group', molecule) -> None:
    """Serialize ``molecule`` (a vtkMolecule) under ``group`` using the
    same layout as C++ ``Tvh5Format::writeMoleculePayload``:

    - Group attrs ``type=molecule``, ``numAtoms``, ``numBonds``.
    - ``atomicNumbers`` — uint16 dataset of length numAtoms.
    - ``atomPositions`` — float32 dataset shaped (numAtoms, 3).
    - ``bondAtoms`` — int64 dataset shaped (numBonds, 2), each row is
      a ``(beginAtomId, endAtomId)`` pair.
    - ``bondOrders`` — uint16 dataset of length numBonds (omitted when
      there are no bonds; reader defaults to 1)."""
    from vtk.util.numpy_support import vtk_to_numpy

    num_atoms = int(molecule.GetNumberOfAtoms())
    num_bonds = int(molecule.GetNumberOfBonds())
    group.attrs['type'] = 'molecule'
    group.attrs['numAtoms'] = np.int64(num_atoms)
    group.attrs['numBonds'] = np.int64(num_bonds)

    if num_atoms > 0:
        atomic_numbers_arr = molecule.GetAtomicNumberArray()
        atomic_numbers = vtk_to_numpy(atomic_numbers_arr).astype(
            np.uint16, copy=False)
        group.create_dataset('atomicNumbers', data=atomic_numbers)
        positions = np.empty((num_atoms, 3), dtype=np.float32)
        for i in range(num_atoms):
            atom = molecule.GetAtom(i)
            pos = atom.GetPosition()
            positions[i, 0] = pos[0]
            positions[i, 1] = pos[1]
            positions[i, 2] = pos[2]
        group.create_dataset('atomPositions', data=positions)

    if num_bonds > 0:
        bond_atoms = np.empty((num_bonds, 2), dtype=np.int64)
        for i in range(num_bonds):
            bond = molecule.GetBond(i)
            bond_atoms[i, 0] = bond.GetBeginAtomId()
            bond_atoms[i, 1] = bond.GetEndAtomId()
        group.create_dataset('bondAtoms', data=bond_atoms)
        orders_arr = molecule.GetBondOrdersArray()
        if orders_arr is not None:
            orders = vtk_to_numpy(orders_arr).astype(np.uint16, copy=False)
            if orders.shape[0] == num_bonds:
                group.create_dataset('bondOrders', data=orders)


def write_state_tvh5(target_path, state_json: dict, pipeline) -> None:
    """Write ``state_json`` plus every populated, non-sink output port
    into ``target_path`` as a ``.tvh5`` HDF5 container.

    For every node N with output port P that carries a Dataset payload
    (volume data), the voxels are written under ``/data/<N>/<P>/`` in
    the same EMD layout as a stand-alone ``.emd`` file. ``Table`` ports
    are written column-by-column under ``/data/<N>/<P>/c<i>`` and
    ``Molecule`` ports as ``atomicNumbers`` / ``atomPositions`` /
    ``bondAtoms`` / ``bondOrders`` datasets, mirroring C++
    ``Tvh5Format::writeTablePayload`` and ``writeMoleculePayload``.
    Either way, a ``dataRef`` entry pointing at the group is stamped
    onto the matching port entry in the JSON before serialization.

    Other payload types (raw scalars, etc.) are skipped with a warning
    — they aren't persisted in the tvh5 container today. The caller
    that wants those leaves on disk should request
    ``output_format='state+port'`` so the per-port writers run
    alongside the tvh5 writer."""
    snapshot = copy.deepcopy(state_json)
    nodes_by_id = {entry['id']: entry for entry in
                   snapshot.get('pipeline', {}).get('nodes') or []
                   if 'id' in entry}

    target_path = Path(target_path)
    with h5py.File(target_path, 'w') as f:
        f.create_group('/data')
        for node in pipeline.nodes:
            if isinstance(node, SinkNode):
                continue
            entry = nodes_by_id.get(node.id)
            if entry is None:
                continue
            outputs = entry.setdefault('outputPorts', {})
            for port in node.output_ports():
                if not port.has_data():
                    continue
                payload = port.data().payload
                is_volume = hasattr(payload, 'arrays')
                is_table = _is_vtk_table(payload)
                is_molecule = _is_vtk_molecule(payload)
                if not (is_volume or is_table or is_molecule):
                    logger.warning(
                        'tvh5 writer: skipping unsupported port %s.%s '
                        '(port type %r) — only volume, table and '
                        'molecule payloads are persisted.',
                        node.label or type(node).__name__, port.name,
                        port.port_type)
                    continue
                node_group_path = f'/data/{node.id}'
                port_group_path = f'{node_group_path}/{port.name}'
                if node_group_path not in f:
                    f.create_group(node_group_path)
                port_group = f.create_group(port_group_path)
                if is_volume:
                    _write_emd_node_into(port_group, payload)
                elif is_table:
                    _write_table_into(port_group, payload)
                else:
                    _write_molecule_into(port_group, payload)
                outputs.setdefault(port.name, {})['dataRef'] = {
                    'container': 'h5',
                    'path': port_group_path,
                }

        # Serialize the (now dataRef-stamped) JSON as an int8 (signed)
        # array at /tomviz_state. The C++ side reads with
        # H5ReadWrite::readData<char>, which strictly H5Tequal-checks
        # the storage type against H5T_STD_I8LE — using uint8 here
        # makes Tvh5Format::readState fall through to an empty state
        # and tomviz then mis-routes the file to LegacyStateLoader.
        # The bytes are identical either way; only the HDF5 type
        # label differs.
        state_bytes = json.dumps(snapshot).encode('utf-8')
        f.create_dataset('tomviz_state',
                         data=np.frombuffer(state_bytes, dtype='i1'))
