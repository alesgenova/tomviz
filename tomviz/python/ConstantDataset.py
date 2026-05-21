import numpy as np

import tomviz.nodes


class ConstantDataset(tomviz.nodes.SourceNode):

    def produce(self, shape=[100, 100, 100], value=0.0):
        ds = self.create_dataset()
        arr = np.full(tuple(shape), value, dtype=np.float32)
        ds.set_scalars("Scalars", arr)
        return {"volume": ds}
