import pytest
from fs.schedule import Schedule


# simple_schedule fixture returns schedules with the minimal args
simple_schedules = [
    ('/foo', '333m', '', 'fs_name', '/foo'),
    ('/foo', '666h', '', 'fs_name', '/foo'),
    ('/foo', '1d', '', 'fs_name', '/foo'),
    ('/foo', '1w', '', 'fs_name', '/foo'),
]
@pytest.fixture(params=simple_schedules)
def simple_schedule(request):
    return Schedule(*request.param)
