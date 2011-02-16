#!/usr/bin/env python

import sys
import subprocess
import re
import math
import itertools

from common import readHosts, runBashScript, AverageEntry
from subprocess import PIPE
from argparse import ArgumentParser

class BalanceTimeLine:
    def __init__(self):
        self.totals = []
        self.transfers = []

class BalanceAverage:
    def __init__(self, bucketsize):
        self.completion = AverageEntry()
        self.bucketsize = bucketsize
        self.xferratio = { }


class BalancerMiner:
    def __init__(self, hostsName, hfilter=None):
        self.hosts = readHosts(hostsName)
        self.hfilter = set(hfilter) if hfilter else None
        self.localhost = (host for host in self.hosts if self.hosts[host]["cores"] == 0).next()

        self.pathRe = re.compile(r"^./([^/]+)/([^/-]+)-(\d+)(-(\d+))?/out-lb.txt$")
        self.totalRe = re.compile(r"^\[([\d.]+)\].*\[(\d+)\] IN TOTAL$")
        self.transferRe = re.compile(r"\[([\d.]+)\].*from (\d+) to (\d+) for (\d+) states$")

    def _logMsg(self, msg):
        print >>sys.stderr, "-- %s" % msg

    def analyzeExperiments(self, explist, skip=1):
        balancedb = { }

        proc = runBashScript("""
           cd %(expdir)s
           for TESTDIR in %(testdirs)s; do            
               find ./$TESTDIR -name 'out-lb.txt' | while read LINE; do
                   echo $LINE
                   cat $LINE | grep "IN TOTAL" | sed -n '1~%(skip)d p; $ p;'
                   cat $LINE | grep "Created transfer request"
               done
           done""" % {
                "testdirs": " ".join(explist),
                "expdir": self.hosts[self.localhost]["expdir"],
                "skip": skip
                }, stdout=PIPE)

        data,_ = proc.communicate()
        lines = data.splitlines()

        for line in lines:
            match = self.pathRe.match(line)
            if match:
                testdir, target, workercount, _, tgcount = match.groups()
                workercount = int(workercount)
                tgcount = int(tgcount) if tgcount else 1

                tgdata = balancedb.setdefault(target, {})
                expdata = tgdata.setdefault((testdir, tgcount), {})
                timeline = expdata.setdefault(workercount, BalanceTimeLine())
                continue
            
            match = self.totalRe.match(line)
            if match:
                timestamp, total = float(match.group(1)), int(match.group(2))
                timeline.totals.append((timestamp, total))
                continue

            match = self.transferRe.match(line)
            if match:
                timestamp = float(match.group(1))
                amount = int(match.group(4))
                timeline.transfers.append((timestamp, amount))

                if amount > 0:
                    if timeline.transfers and abs(timeline.transfers[-1][0] - timestamp) < 0.01:
                        timeline.transfers[-1] = (timestamp, amount + timeline.transfers[-1][1])
                    else:
                        timeline.transfers.append((timestamp, amount))

                continue

            self._logMsg("Unprocessed line: '%s'" % line)

        avgdb = self._averageData(balancedb)

        return avgdb

    def _averageData(self, balancedb, bucketsize=10):
        avgdb = { }
        for target, exps in balancedb.iteritems():
            tgdata = avgdb.setdefault(target, {})
            for tgtrial, exp in exps.iteritems():
                for workercount, timeline in exp.iteritems():
                    average = tgdata.setdefault(workercount, BalanceAverage(bucketsize))
                    # Search for the completion time, if any...
                    try:
                        completion = (entry[0] for entry in timeline.totals if entry[1] == 0).next()
                        average.completion.entries.append(completion)
                    except StopIteration:
                        completion = None

                    # Compute the buckets
                    buckets = { }
                    totiter = iter(timeline.totals)
                    prevtotal, curtotal = None, totiter.next()
                    for transfer in timeline.transfers:
                        while curtotal[0] < transfer[0]:
                            try:
                                prevtotal = curtotal
                                curtotal = totiter.next()
                            except StopIteration:
                                break
                        bucket = buckets.setdefault(int(transfer[0] // bucketsize), AverageEntry())
                        bucket.entries.append(100.0*transfer[1]/prevtotal[1])

                    # Fix this for "gaps" in the buckets
                    for key, avg in buckets.iteritems():
                        avg.computeAverage()
                        avgbucket = average.xferratio.setdefault(key, AverageEntry())
                        avgbucket.entries.append(avg.average)

            for workercount, average in tgdata.iteritems():
                average.completion.computeAverage()
                for xferavg in average.xferratio.itervalues():
                    xferavg.computeAverage()

        return avgdb

    def printAverages(self, avgdb, target="memcached"):
        tgdata = avgdb[target]
        for workercount in sorted(tgdata.keys()):
            average = tgdata[workercount]
            print "%d: %s" % (workercount,
                              "%d" % int(average.completion.average) if average.completion.average else "-"),
            for k in sorted(average.xferratio):
                avgval = average.xferratio[k]
                print "%d=%.2f,%.2f(%d)" % (
                    k,
                    avgval.average,
                    avgval.stdev,
                    len(avgval.entries)
                    ),
            print
            
def main():
    parser = ArgumentParser(description="Mine Cloud9 experiments.",
                            fromfile_prefix_chars="@")
    parser.add_argument("hosts", help="Available cluster machines")
    parser.add_argument("tests", nargs="*", help="Test names")
    parser.add_argument("-f", action="append", help="File containing test names")

    args = parser.parse_args()
    tests = args.tests[:]

    if args.f:
        for fname in args.f:
            f = open(fname, "r")
            tests.extend(f.read().split())
            f.close()

    miner = BalancerMiner(args.hosts)
    results = miner.analyzeExperiments(tests)
    miner.printAverages(results, "memcached")

if __name__ == "__main__":
    main()
