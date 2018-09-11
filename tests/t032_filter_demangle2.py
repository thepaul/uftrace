#!/usr/bin/env python

from runtest import TestBase

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'namespace', lang="C++", result="""
# DURATION    TID     FUNCTION
 128.411 us [30174] | operator new();
   4.580 us [30174] | operator delete();
  10.248 us [30174] | operator new();
   0.317 us [30174] | operator delete();
""", sort='simple')

    def runcmd(self):
        return '%s -F "^operator" %s' % (TestBase.uftrace_cmd, 't-namespace')
