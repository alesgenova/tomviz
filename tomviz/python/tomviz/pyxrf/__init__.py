import os

try:
    from .ic_names import ic_names  # noqa
    from .sids import filter_sids  # noqa
    from .scan_metadata import read_scan_metadata  # noqa
    requirements_installed = True
except ImportError:
    requirements_installed = False

    import traceback
    import_error_exc = traceback.format_exc()

    if 'TOMVIZ_REQUIRE_PYXRF' in os.environ:
        raise


def installed():
    return requirements_installed


def import_error():
    if requirements_installed:
        return ''
    else:
        return import_error_exc
