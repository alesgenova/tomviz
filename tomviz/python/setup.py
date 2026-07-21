from setuptools import setup, find_packages

setup(
    name='tomviz-app',
    version='0.0.1',
    description='Application-side Python layer for the tomviz desktop app '
                '(VTK-backed datasets, in-app operator runtime, file-format '
                'and beamline integrations).',
    author='Kitware, Inc.',
    author_email='kitware@kitware.com',
    url='https://www.tomviz.org/',
    license='BSD 3-Clause',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: BSD 3-Clause',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 3',
    ],
    python_requires='>=3.9',
    packages=find_packages(),
    install_requires=['tomviz-pipeline', 'tqdm', 'h5py', 'numpy', 'scipy'],
    extras_require={
        'itk': ['itk'],
        'pyfftw': ['pyfftw']
    },
)
