def transform(dataset):
    import numpy as np

    array = dataset.active_scalars

    # This is where you operate on your data, here we square root it.
    result = np.sqrt(array)

    # This is where the transformed data is set, it will display in tomviz.
    dataset.active_scalars = result
