#!/usr/bin/env python

from runtest import TestBase
import subprocess as sp
import os, re

TDIR  = 'xxx'
TDIR2 = 'yyy'

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'openclose', """
# DURATION    TID     FUNCTION
   1.088 us [18343] | __monstartup();
   0.640 us [18343] | __cxa_atexit();
            [18343] | main() {
            [18343] |   fopen() {
  86.790 us [18343] |     sys_open();
  89.018 us [18343] |   } /* fopen */
            [18343] |   fclose() {
  10.781 us [18343] |     sys_close();
  37.325 us [18343] |   } /* fclose */
 128.387 us [18343] | } /* main */
""")

    recv_p = None

    def pre(self):
        if os.geteuid() != 0:
            return TestBase.TEST_SKIP
        if os.path.exists('/.dockerenv'):
            return TestBase.TEST_SKIP

        uftrace = TestBase.uftrace_cmd
        program = 't-' + self.name

        recv_cmd = '%s recv -d %s' % (uftrace, TDIR)
        self.recv_p = sp.Popen(recv_cmd.split())

        argument  = '-H %s -k -d %s' % ('localhost', TDIR2)
        argument += ' -N %s@kernel' % '_*do_page_fault'

        record_cmd = '%s record %s %s' % (uftrace, argument, program)
        sp.call(record_cmd.split())
        return TestBase.TEST_SUCCESS

    def runcmd(self):
        return '%s replay -d %s' % (TestBase.uftrace_cmd, os.path.join(TDIR, TDIR2))

    def post(self, ret):
        self.recv_p.terminate()
        sp.call(['rm', '-rf', TDIR])
        return ret

    def fixup(self, cflags, result):
        uname = os.uname()

        # Linux v4.17 (x86_64) changed syscall routines
        major, minor, release = uname[2].split('.')
        if uname[0] == 'Linux' and uname[4] == 'x86_64' and \
           int(major) >= 4 and int(minor) >= 17:
            return re.sub('sys_[a-zA-Z0-9_]+', 'do_syscall_64', result)
        else:
            return result.replace('sys_open', 'sys_openat')
