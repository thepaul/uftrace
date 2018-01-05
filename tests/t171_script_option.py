#!/usr/bin/env python

from runtest import TestBase
import subprocess as sp

FILE='script.py'

script = """
# uftrace-option: -A fopen@arg1/s

def uftrace_entry(ctx):
  if "args" in ctx:
    print("%s(%s)" % (ctx["name"], ctx["args"][0]))

def uftrace_exit(ctx):
  pass
"""

class TestCase(TestBase):
    def __init__(self):
        TestBase.__init__(self, 'openclose', 'fopen(/dev/null)')

    def pre(self):
        f = open(FILE, 'w')
        f.write(script)
        f.close()
        return TestBase.TEST_SUCCESS

    def runcmd(self):
        uftrace = TestBase.ftrace
        options = '-S ' + FILE
        program = 't-' + self.name
        return '%s %s %s' % (uftrace, options, program)

    def sort(self, output):
        return output.strip().split('\n')[0]

    def post(self, ret):
        sp.call(['rm', '-f', FILE])
        return ret
