from __future__ import annotations


def expand_scan_range(scan_range: str, skip_ids: list[int] | None = None) -> list[int]:
    """Expand a scan range string into a sorted list of scan IDs.

    The scan_range uses inclusive-stop slice notation: "start:stop" or
    "start:stop:stride".  Multiple ranges can be comma-separated, and
    individual IDs can be mixed in: "100:110:2,115,120:125".

    After expansion, any IDs in *skip_ids* are removed.
    """
    if not scan_range or not scan_range.strip():
        return []

    skip_set = set(skip_ids) if skip_ids else set()
    ids: set[int] = set()

    for part in scan_range.split(','):
        part = part.strip()
        if not part:
            continue
        if ':' in part:
            pieces = part.split(':')
            start = int(pieces[0])
            stop = int(pieces[1])
            stride = int(pieces[2]) if len(pieces) > 2 else 1
            ids.update(range(start, stop + 1, stride))
        else:
            ids.add(int(part))

    return sorted(ids - skip_set)
