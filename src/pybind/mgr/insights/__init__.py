import os

if 'UNITTEST' not in os.environ:
    import tests

from .module import Module
