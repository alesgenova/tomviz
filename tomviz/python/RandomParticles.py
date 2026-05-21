import numpy as np

import tomviz.nodes


class RandomParticles(tomviz.nodes.SourceNode):

    def produce(self, shape=[128, 128, 128], p_in=30.0, p_s=60.0,
                sparsity=0.20):
        arr = np.zeros(tuple(shape), dtype=np.float32, order='F')

        x = np.fft.fftfreq(shape[0])
        y = np.fft.fftfreq(shape[1])
        z = np.fft.fftfreq(shape[2])

        X, Y, Z = np.meshgrid(y, x, z)
        kr = np.sqrt(X**2 + Y**2 + Z**2)

        # Create inner structure.
        A = np.exp(-p_in * kr)
        phase = np.random.randn(shape[0], shape[1], shape[2])
        F = A * np.exp(2 * np.pi * 1j * phase)
        f = np.fft.ifftn(F)
        f_in = np.absolute(f).copy().flatten()

        # Create shape.
        A = np.exp(-p_s * kr)
        phase = np.random.randn(shape[0], shape[1], shape[2])
        F = A * np.exp(2 * np.pi * 1j * phase)
        f = np.fft.ifftn(F)
        f_shape = np.absolute(f)

        # Impose sparsity (fraction of non-zero voxels).
        f_shape = np.argsort(f_shape, axis=None).flatten()
        n_zero = np.int64(np.round(arr.size * (1 - sparsity)))
        f_shape[n_zero:] = f_shape[n_zero]
        f_in[f_shape] = 0

        f_in = f_in / np.amax(f_in)
        np.copyto(arr, f_in.reshape(arr.shape))

        ds = self.create_dataset()
        ds.set_scalars("Scalars", arr)
        return {"volume": ds}
