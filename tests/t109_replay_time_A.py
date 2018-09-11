#!/usr/bin/env python

from runtest import TestBase
import subprocess as sp

TDIR='xxx'

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'sleep', result="""
# DURATION    TID     FUNCTION
            [32537] | main(1) {
            [32537] |   foo() {
            [32537] |     bar() {
   2.080 ms [32537] |       usleep(2000);
   2.084 ms [32537] |     } /* bar */
   2.102 ms [32537] |   } /* foo */
   2.103 ms [32537] | } /* main */
""", sort='simple')

    def build(self, name, cflags='', ldflags=''):
        if cflags.find('-finstrument-functions') >= 0:
            return TestBase.TEST_SKIP
        return TestBase.build(self, name, cflags, ldflags)

    def pre(self):
        record_cmd = "%s record -A %s -A %s -R %s -d %s %s" % \
                     (TestBase.uftrace_cmd, 'main@arg1', '(malloc|free|usleep)@plt,arg1', \
                      'malloc@retval', TDIR, 't-' + self.name)
        sp.call(record_cmd.split())
        return TestBase.TEST_SUCCESS

    def runcmd(self):
        return '%s replay -t 1ms -d %s' % (TestBase.uftrace_cmd, TDIR)

    def post(self, ret):
        sp.call(['rm', '-rf', TDIR])
        return ret
