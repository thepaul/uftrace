#!/usr/bin/env python

from runtest import TestBase

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'sleep', result="""
# DURATION    TID     FUNCTION
            [18270] | main() {
            [18270] |   foo() {
   2.071 ms [18270] |     bar();
   2.082 ms [18270] |   } /* foo */
   2.083 ms [18270] | } /* main */
""")

    def runcmd(self):
        return '%s -t 1ms -D3 %s' % (TestBase.uftrace_cmd, 't-' + self.name)
