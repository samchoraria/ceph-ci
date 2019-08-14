"""
Thrasher base class
"""
import logging
log = logging.getLogger(__name__)
class Thrasher(object):

    def __init__(self):
        super(Thrasher, self).__init__()
        self.logger = log.getChild('Thrasher')
        self.log("init start")
        self.exception = None
        self.log("init end")

    @property
    def exception(self):
        self.log("property:")
        return self._exception

    @exception.setter
    def exception(self, e):
        self.log("setter:")
        self._exception = e

    def log(self, x):
        """Write data to logger"""
        self.logger.info(x)
