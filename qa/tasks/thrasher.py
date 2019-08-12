"""
Thrasher base class
"""
class Thrasher(object):

    def __init__(self):
        super(Thrasher, self).__init__()
        print "init start"
        self.exception = None
        print "init end"

    @property
    def exception(self):
        print "property"
        return self._exception

    @exception.setter
    def exception(self, e):
        print "setter"
        self._exception = e
