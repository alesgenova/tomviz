###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""CSV writer for vtkTable port payloads. Column names form the header,
each subsequent row is the values across all columns. Numeric arrays are
written via GetTuple1; string arrays via GetValue."""

import csv
from pathlib import Path


def _column_value(column, row: int):
    """Read a single value from a vtkAbstractArray-like column, picking
    GetValue() for string arrays and GetTuple1() for numeric arrays."""
    # vtkStringArray exposes GetValue; vtkDataArray subclasses expose
    # GetTuple1. Try the numeric path first since it's the common case.
    get_tuple = getattr(column, 'GetTuple1', None)
    if get_tuple is not None:
        try:
            return get_tuple(row)
        except (TypeError, ValueError):
            pass
    get_value = getattr(column, 'GetValue', None)
    if get_value is not None:
        return get_value(row)
    raise TypeError(
        f'Unsupported vtkTable column type: {type(column).__name__}')


def write_table_csv(payload, target_path: Path) -> None:
    """Write a vtkTable to CSV. Falls back to a TypeError if the payload
    isn't recognizably tabular."""
    num_columns = getattr(payload, 'GetNumberOfColumns', None)
    num_rows = getattr(payload, 'GetNumberOfRows', None)
    get_column = getattr(payload, 'GetColumn', None)
    if num_columns is None or num_rows is None or get_column is None:
        raise TypeError(
            f'CSV writer expected a vtkTable, got {type(payload).__name__}')

    columns = [get_column(i) for i in range(num_columns())]
    headers = [c.GetName() or f'column_{i}' for i, c in enumerate(columns)]
    rows = num_rows()

    with open(target_path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(headers)
        for r in range(rows):
            w.writerow([_column_value(c, r) for c in columns])
