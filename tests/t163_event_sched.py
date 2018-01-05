#!/usr/bin/env python

from runtest import TestBase

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'sleep', """
# DURATION    TID     FUNCTION
            [  395] | main() {
            [  395] |   foo() {
            [  395] |     bar() {
            [  395] |       usleep() {
   2.088 ms [  395] |         /* linux:schedule */
   2.105 ms [  395] |       } /* usleep */
   2.109 ms [  395] |     } /* bar */
   2.120 ms [  395] |   } /* foo */
   2.121 ms [  395] | } /* main */
""")

    def runcmd(self):
        uftrace = TestBase.ftrace
        options = '-D 2 -F main -F bar -E linux:schedule'
        program = 't-' + self.name
        return '%s %s %s' % (uftrace, options, program)
