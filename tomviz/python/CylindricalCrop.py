def transform(dataset, center_x=-1.0, center_y=-1.0, center_z=-1.0,
              axis_x=0.0, axis_y=0.0, axis_z=1.0,
              radius=-1.0, fill_value=0.0):
    """Crop a volume to a cylindrical region.

    Voxels outside the cylinder are set to fill_value. The cylinder is
    defined by a center point on its axis, an axis direction vector, and
    a radius. The cylinder extends infinitely along its axis (bounded
    only by the volume extent).

    Array convention: shape = (nx, ny, nz), axis 0 = X, 1 = Y, 2 = Z.
    """
    import numpy as np

    array = dataset.active_scalars
    if array is None or array.ndim < 3:
        return

    nx, ny, nz = array.shape

    if center_x < 0:
        center_x = (nx - 1) / 2.0
    if center_y < 0:
        center_y = (ny - 1) / 2.0
    if center_z < 0:
        center_z = (nz - 1) / 2.0

    # Normalize axis direction
    axis = np.array([axis_x, axis_y, axis_z], dtype=np.float64)
    axis_len = np.linalg.norm(axis)
    if axis_len < 1e-12:
        axis = np.array([0.0, 0.0, 1.0])
    else:
        axis = axis / axis_len

    if radius <= 0:
        radius = min(nx, ny) / 2.0

    center = np.array([center_x, center_y, center_z], dtype=np.float64)

    # For each voxel, compute distance to the cylinder axis line.
    # Distance from point P to line through C with unit direction A:
    #   d = || (P - C) - ((P - C) . A) * A ||
    X, Y, Z = np.meshgrid(np.arange(nx), np.arange(ny), np.arange(nz),
                           indexing='ij')

    dx = X - center[0]
    dy = Y - center[1]
    dz = Z - center[2]

    # Project (P - C) onto axis
    proj = dx * axis[0] + dy * axis[1] + dz * axis[2]

    # Perpendicular distance squared
    perp_dist_sq = (dx - proj * axis[0]) ** 2 + \
                   (dy - proj * axis[1]) ** 2 + \
                   (dz - proj * axis[2]) ** 2

    mask = perp_dist_sq > radius ** 2

    result = array.copy()
    result[mask] = fill_value

    dataset.active_scalars = result
