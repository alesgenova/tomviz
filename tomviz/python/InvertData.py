import tomviz.nodes


class InvertData(tomviz.nodes.TransformNode):

    def transform(self, inputs):
        dataset = inputs["volume"]
        self.progress.maximum = max(1, dataset.num_scalars)

        output = dataset.apply_to_each_scalar_array(self._invert_scalars)

        if self.canceled:
            return

        return {"volume": output}

    def _invert_scalars(self, scalars):
        import numpy as np

        if self.canceled:
            return
        
        arr = np.float32(scalars)
        self.progress.value += 1
        return np.amax(arr) - arr + np.amin(arr)
