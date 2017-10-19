

from mgr_test_case import MgrTestCase

import logging
import requests


log = logging.getLogger(__name__)


class TestDashboard(MgrTestCase):
    MGRS_REQUIRED = 3

    def _assign_ports(self):
        """
        To avoid the need to run lots of hosts in teuthology tests to
        get different URLs per mgr, we will hand out different ports
        to each mgr here.

        This is already taken care of for us when running in a vstart
        environment.
        """
        # Start handing out ports well above Ceph's range.
        dashboard_port = 6789 + 1000

        for mgr_id in self.mgr_cluster.mgr_ids:
            self.mgr_cluster.mgr_stop(mgr_id)

        for mgr_id in self.mgr_cluster.mgr_ids:
            log.info("Using port {0} for dashboard on mgr.{1}".format(
                dashboard_port, mgr_id
            ))
            self.mgr_cluster.set_module_localized_conf("dashboard", mgr_id,
                                                       "server_port",
                                                       str(dashboard_port))
            dashboard_port += 1

        for mgr_id in self.mgr_cluster.mgr_ids:
            self.mgr_cluster.mgr_restart(mgr_id)

    def test_standby(self):
        self._assign_ports()
        self._load_module("dashboard")

        original_active = self.mgr_cluster.get_active_id()

        original_uri = self._get_uri("dashboard")
        log.info("Originally running at {0}".format(original_uri))

        self.mgr_cluster.mgr_fail(original_active)

        failed_over_uri = self._get_uri("dashboard")
        log.info("After failover running at {0}".format(original_uri))

        self.assertNotEqual(original_uri, failed_over_uri)

        # The original active daemon should have come back up as a standby
        # and be doing redirects to the new active daemon
        r = requests.get(original_uri, allow_redirects=False)
        self.assertEqual(r.status_code, 303)

    def test_urls(self):
        self._load_module("dashboard")

        base_uri = self._get_uri("dashboard")

        # This is a very simple smoke test to check that the dashboard can
        # give us a 200 response to requests.  We're not testing that
        # the content is correct or even renders!

        urls = [
            "/health",
            "/servers",
            "/osd",
            "/osd/perf/0",
            "/rbd_mirroring",
            "/rbd_iscsi"
        ]

        failures = []

        for url in urls:
            r = requests.get(base_uri + url)
            if r.status_code != 200:
                failures.append(url)

            log.info("{0}: {1} ({2} bytes)".format(
                url, r.status_code, len(r.content)
            ))

        self.assertListEqual(failures, [])
