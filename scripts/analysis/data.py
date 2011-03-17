#!/usr/bin/env python

from __future__ import with_statement

import os
import sys
import itertools
import numpy

from StringIO import StringIO

class ClassDict(dict):
    def __setitem__(self, k, v):
        super(ClassDict, self).__setitem__(k, v)
        if not hasattr(dict, k):
            setattr(self, k, v)

class Experiment(object):
    def __init__(self, klee_out_dir, errlog = sys.stderr):
        self.errlog = errlog
        self.klee_out_dir = klee_out_dir
        self.name = klee_out_dir
        self.run_stats_file = None
        self.open_run_stats_file()
        self.update_run_stats_file()

    def update(self):
        self.update_run_stats_file()

    def open_run_stats_file(self):
        """ Open run.stats file and read it's header """
        run_stats_fname = os.path.join(self.klee_out_dir, 'all.stats')
        try:
            self.run_stats_file = open(run_stats_fname, 'r')
        except IOError:
            self.errlog.write('Can not open %s\n' % run_stats_fname)
            self.run_stats_file = None
            return

        line = self.run_stats_file.readline()
        if not line or not line.startswith("('") or not line.endswith(')\n'):
            self.errlog.write('Can not read the header line from %s\n' \
                              % run_stats_fname)
            self.run_stats_file.close()
            self.run_stats_file = None
            return

        self.run_stats_headers = [x[1:-1] for x in line[1:-2].split(',') if x]
        self.headers = self.run_stats_headers
        self.stats = ClassDict()
        for k in self.run_stats_headers:
            self.stats[k] = []

    def update_run_stats_file(self):
        """ Incrementally read newly added lines from run.stats """

        if not self.run_stats_file:
            return

        # We can not use file as an iterator since it does not resumes
        # reading after reading EOF even if the file is updated
        line = self.run_stats_file.readline()
        while line:
            assert line.startswith('(') and line.endswith(')\n')

            line_stats = line[1:-2].split(',')
            assert len(line_stats) == len(self.run_stats_headers)

            for k, v in itertools.izip(self.run_stats_headers, line_stats):
                self.stats[k].append(float(v))

            line = self.run_stats_file.readline()

        self.a = ClassDict()
        for k, v in self.stats.iteritems():
            self.a[k] = numpy.array(v, dtype=numpy.double)

    def __getitem__(self, key):
        return self.stats[key]

    def __repr__(self):
        return "<%s '%s'>" % (self.__class__.__name__, self.name)

if __name__ == '__main__':
    if len(sys.argv) > 1:
        exp = Experiment(sys.argv[1])
    from IPython.Shell import IPShellEmbed
    ipshell = IPShellEmbed()
    ipshell()

