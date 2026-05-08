"""Create a synthetic tomo.h5 file for testing PyXRFSource."""
import sys

import h5py
import numpy as np


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <output_path>", file=sys.stderr)
        sys.exit(1)

    output_path = sys.argv[1]

    num_angles = 5
    num_elements = 3
    num_rows = 8
    num_cols = 10

    element_names = [b'Fe_K', b'Cu_K', b'Zn_K']

    np.random.seed(42)
    data = np.random.rand(num_angles, num_elements, num_rows, num_cols)
    theta = np.linspace(-90, 90, num_angles)

    with h5py.File(output_path, 'w') as f:
        f.create_group('exchange')
        f['exchange'].create_dataset('theta', data=theta)
        f.create_group('reconstruction/fitting')
        f['reconstruction/fitting'].create_dataset('data', data=data)
        f['reconstruction/fitting'].create_dataset(
            'elements', data=np.array(element_names, dtype='S20'))

    print(f"Created synthetic tomo.h5 at {output_path}")


if __name__ == '__main__':
    main()
