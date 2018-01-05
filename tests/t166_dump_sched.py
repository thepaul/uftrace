#!/usr/bin/env python

from runtest import TestBase
import subprocess as sp

TDIR='xxx'

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'sleep', """
{"traceEvents":[
{"ts":112150305218.363,"ph":"B","pid":306,"name":"__monstartup"},
{"ts":112150305220.090,"ph":"E","pid":306,"name":"__monstartup"},
{"ts":112150305224.313,"ph":"B","pid":306,"name":"__cxa_atexit"},
{"ts":112150305225.219,"ph":"E","pid":306,"name":"__cxa_atexit"},
{"ts":112150305226.496,"ph":"B","pid":306,"name":"main"},
{"ts":112150305226.752,"ph":"B","pid":306,"name":"foo"},
{"ts":112150305226.825,"ph":"B","pid":306,"name":"mem_alloc"},
{"ts":112150305226.921,"ph":"B","pid":306,"name":"malloc"},
{"ts":112150305227.641,"ph":"E","pid":306,"name":"malloc"},
{"ts":112150305228.173,"ph":"E","pid":306,"name":"mem_alloc"},
{"ts":112150305228.317,"ph":"B","pid":306,"name":"bar"},
{"ts":112150305228.436,"ph":"B","pid":306,"name":"usleep"},
{"ts":112150305241.755,"ph":"B","pid":306,"name":"linux:schedule"},
{"ts":112150307301.727,"ph":"E","pid":306,"name":"linux:schedule"},
{"ts":112150307318.143,"ph":"E","pid":306,"name":"usleep"},
{"ts":112150307321.099,"ph":"E","pid":306,"name":"bar"},
{"ts":112150307322.007,"ph":"B","pid":306,"name":"mem_free"},
{"ts":112150307323.132,"ph":"B","pid":306,"name":"free"},
{"ts":112150307328.403,"ph":"E","pid":306,"name":"free"},
{"ts":112150307328.615,"ph":"E","pid":306,"name":"mem_free"},
{"ts":112150307328.742,"ph":"E","pid":306,"name":"foo"},
{"ts":112150307328.905,"ph":"E","pid":306,"name":"main"}
], "displayTimeUnit": "ns", "metadata": {
"command_line":"uftrace record -d xxx -E linux:schedule t-sleep ",
"recorded_time":"Fri Aug 25 14:23:29 2017"
} }
""", sort='chrome')

    def pre(self):
        options = '-d %s -E %s' % (TDIR, 'linux:schedule')
        record_cmd = '%s record %s %s' % (TestBase.ftrace, options, 't-' + self.name)
        sp.call(record_cmd.split())
        return TestBase.TEST_SUCCESS

    def runcmd(self):
        return '%s dump -d %s -F main --chrome' % (TestBase.ftrace, TDIR)

    def post(self, ret):
        sp.call(['rm', '-rf', TDIR])
        return ret
