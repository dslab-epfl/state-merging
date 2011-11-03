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

        self.is_loaded = False      # Is experiment info loaded successfuly ?

        self.headers = []           # Headers
        self.stats = ClassDict()    # Stats map

        self.cmdline = ''           # Command line
        self.opts = ClassDict()     # Parsed command line options (dict)
        self.is_done = False        # Is experiment complete ?

        self.update()

    def update(self):
        self.update_run_stats_file()
        self.update_info_file()

    def update_run_stats_file(self):
        """ Re-read run.stats file """

        self.is_loaded = False

        self.headers = []
        self.stats = ClassDict()

        # Open file
        run_stats_fname = os.path.join(self.klee_out_dir, 'all.stats')
        try:
            run_stats_file = open(run_stats_fname, 'r')
        except IOError:
            self.errlog.write('Can not open %s\n' % run_stats_fname)
            return

        with run_stats_file:
            # Read the header
            line = run_stats_file.readline()
            if not line or not line.startswith("('") or not line.endswith(')\n'):
                self.errlog.write('Can not read the header line from %s\n' \
                                  % run_stats_fname)
                return

            self.headers = [x[1:-1] for x in line[1:-2].split(',') if x]
            self.stats = ClassDict()
            for k in self.headers:
                self.stats[k] = []

            # Read the data
            for line in run_stats_file:
                assert line.startswith('(') and line.endswith(')\n')

                line_stats = line[1:-2].split(',')
                assert len(line_stats) == len(self.headers)

                for k, v in itertools.izip(self.headers, line_stats):
                    self.stats[k].append(float(v))

            # Convert lists to numpy arrays
            for k in self.stats.keys():
                self.stats[k] = numpy.array(self.stats[k], dtype=numpy.double)

            # Recompute InstructionsMult if possible
            try:
                self.stats['InstructionsMultApprox'] = \
                            self.stats.InstructionsMult + \
                            self.stats.InstructionsMultHigh * float(2**64)
            except:
                pass

            self.is_loaded = True

    def update_info_file(self):
        self.is_loaded = False

        self.cmdline = ''
        self.opts = ClassDict()
        self.is_done = False

        info_file_fname = os.path.join(self.klee_out_dir, 'info')
        try:
            info_file = open(info_file_fname, 'r')
        except IOError:
            self.errlog.write('Can not open %s\n' % info_file_fname)
            return

        with info_file:
            self.cmdline = info_file.readline()

            # XXX
            self.cmdline=self.cmdline.replace('--output-dir ', '--output-dir=')

            self.opts = ClassDict()
            for opt in self.cmdline.split(' ')[1:]:
                if not opt.startswith('-'):
                    break
                if opt.startswith('--'):
                    opt = opt[2:]
                elif opt.startswith('-'):
                    opt = opt[1:]
                
                opt = opt.split('=')
                optname = opt[0].replace('-', '_')
                if len(opt) == 1:
                    self.opts[optname] = '1'
                else:
                    self.opts[optname] = opt[1]

            self.is_done = False

            for line in info_file:
                if line.startswith('Finished:'):
                    self.is_done = True

            self.is_loaded = True

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

