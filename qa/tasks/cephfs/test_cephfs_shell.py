"""
Task for running foo
"""
from cStringIO import StringIO
import logging
import unittest
import sys
sys.path.insert(0, '../../../src/tools/cephfs')
from unittest.mock import create_autospec
import cephfs_shell as CephFSShell
from teuthology import misc
from teuthology.exceptions import ConfigError
from teuthology.task import Task
from teuthology.orchestra import run
from teuthology.orchestra.remote import Remote

log = logging.getLogger(__name__)


class CephFSShellTest(Task):
    """
    Install and mount foo
    This will require foo.
    For example:
    tasks:
    - foo:
        biz:
        bar:
    Possible options for this task are:
        biz:
        bar:
        baz:
    """
    def __init__(self, ctx, config):
        super(CephFSShellTest, self).__init__(ctx, config)
        self.log = log
        log.info('In __init__ step, hello world')

    def setup(self):
        super(CephFSShellTest, self).setup()
        config = self.config
        log.info('In setup step, hello world')
        log.debug('config is: %r', config)
        self.mock_stdin = create_autospec(sys.stdin)
        self.mock_stdout = create_autospec(sys.stdout)

    def create(self, server=None):
        return CephFSShell(stdin=self.mock_stdin, stdout=self.mock_stdout)

    def _last_write(self, nr=None):
        """:return: last `n` output lines"""
        if nr is None:
            return self.mock_stdout.write.call_args[0][0]
        return "".join(map(lambda c: c[0][0], self.mock_stdout.write.call_args_list[nr:]))

    def test_umask(self):
        """Tesing `umask` command"""
        cli = self.create()
        self.assertFalse(cli.onecmd("umask"))
        self.assertTrue(self.mock_stdout.flush.called)
        self.assertEqual("0002\n", self._last_write())

    def begin(self):
        super(CephFSShellTest, self).begin()
        log.info('In begin step, hello world')
        ctx = self.ctx
        log.debug('ctx is: %r', ctx)
        remote = Remote('rpavani1998@teuthology.front.sepia.ceph.com')
        remote.run(args=['echo','"hello world: console output 15"'], stdout=StringIO())
        remote.run(args=['sleep', '15'], stdout=StringIO())

    def teardown(self):
        log.info('Teardown step, hello world')

task = CephFSShellTest