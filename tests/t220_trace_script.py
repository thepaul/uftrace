#!/usr/bin/env python

import os, stat
from runtest import TestBase

TEST_SCRIPT = "./test-script.sh"

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'abc', """
# DURATION    TID     FUNCTION
 133.697 us [28137] | fork();
            [28141] | } /* fork */
            [28141] | main() {
            [28141] |   a() {
            [28141] |     b() {
            [28141] |       c() {
   0.753 us [28141] |         getpid();
   1.430 us [28141] |       } /* c */
   1.915 us [28141] |     } /* b */
   2.405 us [28141] |   } /* a */
   3.005 us [28141] | } /* main */
""", sort='simple')

    def pre(self):
        f = open(TEST_SCRIPT, "w")
        f.write("""#!/bin/sh
./t-abc
""")
        f.close()
        os.chmod(TEST_SCRIPT, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
        return TestBase.TEST_SUCCESS

    def runcmd(self):
        uftrace = TestBase.uftrace_cmd
        options = "--force -F fork -F main"
        program = TEST_SCRIPT

        return "%s %s %s" % (uftrace, options, program)

    def post(self, result):
        os.remove(TEST_SCRIPT)
        return result

