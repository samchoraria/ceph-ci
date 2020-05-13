from typing import NamedTuple, List
import pytest

from ceph.deployment.service_spec import ServiceSpec, PlacementSpec, ServiceSpecValidationError

from cephadm.module import HostAssignment
from orchestrator import DaemonDescription, OrchestratorValidationError


def wrapper(func):
    # some odd thingy to revert the order or arguments
    def inner(*args):
        def inner2(expected):
            func(expected, *args)
        return inner2
    return inner


@wrapper
def none(expected):
    assert expected == []


@wrapper
def one_of(expected, *hosts):
    if not isinstance(expected, list):
        assert False, str(expected)
    assert len(expected) == 1, f'one_of failed len({expected}) != 1'
    assert expected[0] in hosts


@wrapper
def two_of(expected, *hosts):
    if not isinstance(expected, list):
        assert False, str(expected)
    assert len(expected) == 2, f'one_of failed len({expected}) != 2'
    matches = 0
    for h in hosts:
        matches += int(h in expected)
    if matches != 2:
        assert False, f'two of {hosts} not in {expected}'


@wrapper
def exactly(expected, *hosts):
    assert expected == list(hosts)


@wrapper
def error(expected, kind, match):
    assert isinstance(expected, kind), (str(expected), match)
    assert str(expected) == match, (str(expected), match)


@wrapper
def _or(expected, *inners):
    def catch(inner):
        try:
            inner(expected)
        except AssertionError as e:
            return e
    result = [catch(i) for i in inners]
    if None not in result:
        assert False, f"_or failed: {expected}"


def _always_true(_): pass


def k(s):
    return [e for e in s.split(' ') if e]


def get_result(key, results):
    def match(one):
        for o, k in zip(one, key):
            if o != k and o != '*':
                return False
        return True
    return [v for k, v in results
     if match(k)][0]


# * first match from the top wins
# * where e=[], *=any
#
#       + list of known hosts available for scheduling
#       |   + hosts used for explict placement
#       |   |   + count
#       |   |   |
test_explicit_scheduler_results = [
    (k("*   *   0"), error(ServiceSpecValidationError, 'num/count must be > 1')),
    (k("*   e   N"), error(OrchestratorValidationError, 'placement spec is empty: no hosts, no label, no pattern, no count')),
    (k("*   e   *"), none),
    (k("e   1   *"), error(OrchestratorValidationError, "Cannot place <ServiceSpec for service_name=mon> on 1: Unknown hosts")),
    (k("e   12  *"), error(OrchestratorValidationError, "Cannot place <ServiceSpec for service_name=mon> on 1, 2: Unknown hosts")),
    (k("e   123 *"), error(OrchestratorValidationError, "Cannot place <ServiceSpec for service_name=mon> on 1, 2, 3: Unknown hosts")),
    (k("1   1   *"), exactly('1')),
    (k("1   12  *"), error(OrchestratorValidationError, "Cannot place <ServiceSpec for service_name=mon> on 2: Unknown hosts")),
    (k("1   123 *"), error(OrchestratorValidationError, "Cannot place <ServiceSpec for service_name=mon> on 2, 3: Unknown hosts")),
    (k("12  1   *"), exactly('1')),
    (k("12  12  1"), one_of('1', '2')),
    (k("12  12  *"), exactly('1', '2')),
    (k("12  123 *"), error(OrchestratorValidationError, "Cannot place <ServiceSpec for service_name=mon> on 3: Unknown hosts")),
    (k("123 1   *"), exactly('1')),
    (k("123 12  1"), one_of('1', '2')),
    (k("123 12  *"), exactly('1', '2')),
    (k("123 123 1"), one_of('1', '2', '3')),
    (k("123 123 2"), two_of('1', '2', '3')),
    (k("123 123 *"), exactly('1', '2', '3')),
]

@pytest.mark.parametrize("count",
    [
        None,
        0,
        1,
        2,
        3,
    ])
@pytest.mark.parametrize("explicit_key, explicit",
    [
        ('e', []),
        ('1', ['1']),
        ('12', ['1', '2']),
        ('123', ['1', '2', '3']),
    ])
@pytest.mark.parametrize("host_key, hosts",
    [
        ('e', []),
        ('1', ['1']),
        ('12', ['1', '2']),
        ('123', ['1', '2', '3']),
    ])
def test_scheduler(host_key, hosts,
                   explicit_key, explicit,
                   count):
    count_key = 'N' if count is None else str(count)
    key = k(f'{host_key} {explicit_key} {count_key}')
    try:
        assert_res = get_result(key, test_explicit_scheduler_results)
    except IndexError:
        assert False, f'`(k("{host_key} {explicit_key} {count_key}"), ...),` not found'

    for _ in range(10):  # scheduler has a random component
        try:
            host_res = HostAssignment(
                spec=ServiceSpec('mon', placement=PlacementSpec(
                    hosts=explicit,
                    count=count,
                )),
                get_hosts_func=lambda _: hosts,
                get_daemons_func=lambda _: []).place()

            assert_res(sorted([h.hostname for h in host_res]))
        except Exception as e:
            assert_res(e)


class NodeAssignmentTest(NamedTuple):
    service_type: str
    placement: PlacementSpec
    hosts: List[str]
    daemons: List[DaemonDescription]
    expected: List[str]

@pytest.mark.parametrize("service_type,placement,hosts,daemons,expected",
    [
        # just hosts
        NodeAssignmentTest(
            'mon',
            PlacementSpec(hosts=['smithi060:[v2:172.21.15.60:3301,v1:172.21.15.60:6790]=c']),
            ['smithi060'],
            [],
            ['smithi060']
        ),
        # all_hosts
        NodeAssignmentTest(
            'mon',
            PlacementSpec(host_pattern='*'),
            'host1 host2 host3'.split(),
            [
                DaemonDescription('mon', 'a', 'host1'),
                DaemonDescription('mon', 'b', 'host2'),
            ],
            ['host1', 'host2', 'host3']
        ),
        # count that is bigger than the amount of hosts. Truncate to len(hosts)
        # RGWs should not be co-located to each other.
        NodeAssignmentTest(
            'rgw',
            PlacementSpec(count=4),
            'host1 host2 host3'.split(),
            [],
            ['host1', 'host2', 'host3']
        ),
        # count + partial host list
        NodeAssignmentTest(
            'mon',
            PlacementSpec(count=3, hosts=['host3']),
            'host1 host2 host3'.split(),
            [
                DaemonDescription('mon', 'a', 'host1'),
                DaemonDescription('mon', 'b', 'host2'),
            ],
            ['host3']
        ),
        # count 1 + partial host list
        NodeAssignmentTest(
            'mon',
            PlacementSpec(count=1, hosts=['host3']),
            'host1 host2 host3'.split(),
            [
                DaemonDescription('mon', 'a', 'host1'),
                DaemonDescription('mon', 'b', 'host2'),
            ],
            ['host3']
        ),
        # count + partial host list + existing
        NodeAssignmentTest(
            'mon',
            PlacementSpec(count=2, hosts=['host3']),
            'host1 host2 host3'.split(),
            [
                DaemonDescription('mon', 'a', 'host1'),
            ],
            ['host3']
        ),
        # count + partial host list + existing (deterministic)
        NodeAssignmentTest(
            'mon',
            PlacementSpec(count=2, hosts=['host1']),
            'host1 host2'.split(),
            [
                DaemonDescription('mon', 'a', 'host1'),
            ],
            ['host1']
        ),
        # count + partial host list + existing (deterministic)
        NodeAssignmentTest(
            'mon',
            PlacementSpec(count=2, hosts=['host1']),
            'host1 host2'.split(),
            [
                DaemonDescription('mon', 'a', 'host2'),
            ],
            ['host1']
        ),
        # label only
        NodeAssignmentTest(
            'mon',
            PlacementSpec(label='foo'),
            'host1 host2 host3'.split(),
            [],
            ['host1', 'host2', 'host3']
        ),
        # host_pattern
        NodeAssignmentTest(
            'mon',
            PlacementSpec(host_pattern='mon*'),
            'monhost1 monhost2 datahost'.split(),
            [],
            ['monhost1', 'monhost2']
        ),
    ])
def test_node_assignment(service_type, placement, hosts, daemons, expected):
    hosts = HostAssignment(
        spec=ServiceSpec(service_type, placement=placement),
        get_hosts_func=lambda _: hosts,
        get_daemons_func=lambda _: daemons).place()
    assert sorted([h.hostname for h in hosts]) == sorted(expected)

class NodeAssignmentTest2(NamedTuple):
    service_type: str
    placement: PlacementSpec
    hosts: List[str]
    daemons: List[DaemonDescription]
    expected_len: int
    in_set: List[str]

@pytest.mark.parametrize("service_type,placement,hosts,daemons,expected_len,in_set",
    [
        # just count
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=1),
            'host1 host2 host3'.split(),
            [],
            1,
            ['host1', 'host2', 'host3'],
        ),

        # hosts + (smaller) count
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=1, hosts='host1 host2'.split()),
            'host1 host2'.split(),
            [],
            1,
            ['host1', 'host2'],
        ),
        # hosts + (smaller) count, existing
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=1, hosts='host1 host2 host3'.split()),
            'host1 host2 host3'.split(),
            [DaemonDescription('mon', 'mon.a', 'host1'),],
            1,
            ['host1', 'host2', 'host3'],
        ),
        # hosts + (smaller) count, (more) existing
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=1, hosts='host1 host2 host3'.split()),
            'host1 host2 host3'.split(),
            [
                DaemonDescription('mon', 'a', 'host1'),
                DaemonDescription('mon', 'b', 'host2'),
            ],
            1,
            ['host1', 'host2']
        ),
        # count + partial host list
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=2, hosts=['host3']),
            'host1 host2 host3'.split(),
            [],
            1,
            ['host1', 'host2', 'host3']
        ),
        # label + count
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=1, label='foo'),
            'host1 host2 host3'.split(),
            [],
            1,
            ['host1', 'host2', 'host3']
        ),
    ])
def test_node_assignment2(service_type, placement, hosts,
                          daemons, expected_len, in_set):
    hosts = HostAssignment(
        spec=ServiceSpec(service_type, placement=placement),
        get_hosts_func=lambda _: hosts,
        get_daemons_func=lambda _: daemons).place()
    assert len(hosts) == expected_len
    for h in [h.hostname for h in hosts]:
        assert h in in_set

@pytest.mark.parametrize("service_type,placement,hosts,daemons,expected_len,must_have",
    [
        # hosts + (smaller) count, (more) existing
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=3, hosts='host3'.split()),
            'host1 host2 host3'.split(),
            [],
            1,
            ['host3']
        ),
        # count + partial host list
        NodeAssignmentTest2(
            'mon',
            PlacementSpec(count=2, hosts=['host3']),
            'host1 host2 host3'.split(),
            [],
            1,
            ['host3']
        ),
    ])
def test_node_assignment3(service_type, placement, hosts,
                          daemons, expected_len, must_have):
    hosts = HostAssignment(
        spec=ServiceSpec(service_type, placement=placement),
        get_hosts_func=lambda _: hosts,
        get_daemons_func=lambda _: daemons).place()
    assert len(hosts) == expected_len
    for h in must_have:
        assert h in [h.hostname for h in hosts]


@pytest.mark.parametrize("placement",
    [
        ('1 *'),
        ('* label:foo'),
        ('* host1 host2'),
        ('hostname12hostname12hostname12hostname12hostname12hostname12hostname12'),  # > 63 chars
    ])
def test_bad_placements(placement):
    try:
        s = PlacementSpec.from_string(placement.split(' '))
        assert False
    except ServiceSpecValidationError as e:
        pass


class NodeAssignmentTestBadSpec(NamedTuple):
    service_type: str
    placement: PlacementSpec
    hosts: List[str]
    daemons: List[DaemonDescription]
    expected: str
@pytest.mark.parametrize("service_type,placement,hosts,daemons,expected",
    [
        # unknown host
        NodeAssignmentTestBadSpec(
            'mon',
            PlacementSpec(hosts=['unknownhost']),
            ['knownhost'],
            [],
            "Cannot place <ServiceSpec for service_name=mon> on unknownhost: Unknown hosts"
        ),
        # unknown host pattern
        NodeAssignmentTestBadSpec(
            'mon',
            PlacementSpec(host_pattern='unknownhost'),
            ['knownhost'],
            [],
            "Cannot place <ServiceSpec for service_name=mon>: No matching hosts"
        ),
        # unknown label
        NodeAssignmentTestBadSpec(
            'mon',
            PlacementSpec(label='unknownlabel'),
            [],
            [],
            "Cannot place <ServiceSpec for service_name=mon>: No matching hosts for label unknownlabel"
        ),
    ])
def test_bad_specs(service_type, placement, hosts, daemons, expected):
    with pytest.raises(OrchestratorValidationError) as e:
        hosts = HostAssignment(
            spec=ServiceSpec(service_type, placement=placement),
            get_hosts_func=lambda _: hosts,
            get_daemons_func=lambda _: daemons).place()
    assert str(e.value) == expected
