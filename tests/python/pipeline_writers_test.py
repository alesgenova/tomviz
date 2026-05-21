###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Tests for the per-port-type output writers (CSV for tables, XYZ for
molecules, plus the registry plumbing). Build the VTK objects by hand
so we don't need an operator end-to-end."""

import csv

import pytest
import vtk

from tomviz.pipeline.writers import (
    register_writer,
    writer_for,
    write_molecule_xyz,
    write_table_csv,
)


def _build_table():
    """A vtkTable with a numeric column and a string column."""
    t = vtk.vtkTable()

    radius = vtk.vtkFloatArray()
    radius.SetName('radius')
    radius.SetNumberOfComponents(1)
    radius.SetNumberOfTuples(3)
    for i, v in enumerate([1.5, 2.0, 3.25]):
        radius.SetValue(i, v)
    t.AddColumn(radius)

    label = vtk.vtkStringArray()
    label.SetName('label')
    label.SetNumberOfValues(3)
    for i, s in enumerate(['small', 'medium', 'large']):
        label.SetValue(i, s)
    t.AddColumn(label)

    return t


def _build_methane_molecule():
    """vtkMolecule for methane (CH4) with one bond per H — bonds aren't
    written by the XYZ writer but we add them to verify they're tolerated."""
    m = vtk.vtkMolecule()
    # central carbon at origin, four hydrogens at unit distance
    m.AppendAtom(6, 0.0, 0.0, 0.0)
    m.AppendAtom(1, 1.0, 0.0, 0.0)
    m.AppendAtom(1, -1.0, 0.0, 0.0)
    m.AppendAtom(1, 0.0, 1.0, 0.0)
    m.AppendAtom(1, 0.0, -1.0, 0.0)
    for h in range(1, 5):
        m.AppendBond(0, h, 1)
    return m


def test_table_csv_round_trip(tmp_path):
    target = tmp_path / 'out.csv'
    write_table_csv(_build_table(), target)
    with open(target, newline='') as f:
        rows = list(csv.reader(f))
    assert rows[0] == ['radius', 'label']
    # Numeric values come out as Python floats (str-formatted by csv).
    assert rows[1] == ['1.5', 'small']
    assert rows[2] == ['2.0', 'medium']
    assert rows[3] == ['3.25', 'large']


def test_table_csv_rejects_non_table(tmp_path):
    with pytest.raises(TypeError):
        write_table_csv({'not': 'a table'}, tmp_path / 'x.csv')


def test_molecule_xyz_writes_methane(tmp_path):
    target = tmp_path / 'methane.xyz'
    write_molecule_xyz(_build_methane_molecule(), target)
    lines = target.read_text().splitlines()
    assert lines[0] == '5'
    # Comment line is freeform; just make sure it's there.
    assert lines[1]
    # Five atom lines starting with element symbols.
    symbols = [line.split()[0] for line in lines[2:7]]
    assert symbols == ['C', 'H', 'H', 'H', 'H']
    # First C is at the origin.
    parts = lines[2].split()
    assert float(parts[1]) == pytest.approx(0.0)
    assert float(parts[2]) == pytest.approx(0.0)
    assert float(parts[3]) == pytest.approx(0.0)


def test_molecule_xyz_rejects_non_molecule(tmp_path):
    with pytest.raises(TypeError):
        write_molecule_xyz({'not': 'a molecule'}, tmp_path / 'x.xyz')


def test_registry_lookup():
    # Built-in port types resolve to their declared writers.
    assert writer_for('Volume')[0] == 'emd'
    assert writer_for('TiltSeries')[0] == 'emd'
    assert writer_for('Table')[0] == 'csv'
    assert writer_for('Molecule')[0] == 'xyz'
    # Unknown types return None so the CLI can skip with a warning.
    assert writer_for('Bogus') is None


def test_register_writer_overrides(tmp_path):
    sentinel = []

    def custom(payload, target):
        sentinel.append((payload, target))
        target.write_text('ok')

    try:
        register_writer('Bogus', 'bin', custom)
        ext, w = writer_for('Bogus')
        assert ext == 'bin'
        target = tmp_path / 'out.bin'
        w('payload', target)
        assert target.read_text() == 'ok'
        assert sentinel == [('payload', target)]
    finally:
        # Clean up — the registry is process-global.
        from tomviz.pipeline.writers import _REGISTRY
        _REGISTRY.pop('Bogus', None)
