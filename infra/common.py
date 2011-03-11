"""
Common functionality across all the Python scripts.
"""

import subprocess
import math

_HOSTS_DIR = "./hosts"
_CMDLINES_DIR = "./cmdlines"
_EXP_DIR = "./exp"
_KLEECMD_DIR = "./kleecmd"
_COVERABLE_DIR = "./coverable"

class AverageEntry:
    def __init__(self):
        self.entries = []
        self.average = None
        self.stdev = None

    def _computeAverage(self, entries):
        average = sum(entries)/len(entries) if len(entries) else 0.0
        stdev = math.sqrt(sum([(x-average)*(x-average) for x in entries])/
                               (len(entries)-1)) if len(entries) > 1 else 0.0

        return average,stdev

    def computeAverage(self, fixoutliers=False):
        average, stdev = self._computeAverage(self.entries)
        if not fixoutliers or len(self.entries) < 4:
            self.average, self.stdev = average, stdev
            return
        
        newentries = filter(lambda x: abs(x-average) < stdev, self.entries)
        self.average, self.stdev = self._computeAverage(newentries)

    def __str__(self):
        return "(%s,%s)(%d)" % (
            "%.2f" % self.average if self.average else "-",
            "%.2f" % self.stdev if self.stdev else "-",
            len(self.entries)
            )
    def __repr__(self):
        return self.__str__()


def _getHostsPath(hosts):
    return "%s/%s.hosts" % (_HOSTS_DIR, hosts)

def _getCmdlinesPath(cmdlines):
    return "%s/%s.cmdlines" % (_CMDLINES_DIR, cmdlines)

def _getExpPath(exp):
    return "%s/%s.exp" % (_EXP_DIR, exp)

def _getKleeCmdPath(kleeCmd):
    return "%s/%s.kcmd" % (_KLEECMD_DIR, kleeCmd)

def runBashScript(script, **extra):
    return subprocess.Popen(["/bin/bash", "-c", script], **extra)

def getCoverablePath(coverable):
    return "%s/%s.coverable" % (_COVERABLE_DIR, coverable)

def readHosts(hostsFile):
    hosts = { }
    f = open(_getHostsPath(hostsFile), "r")
    for line in f:
        if line.startswith("#"):
            continue
        tokens = line.split()
        host = tokens[0]
        hosts[host] = dict(zip(["cores", "root", "user", "expdir", "targetdir"], tokens[1:]))
        hosts[host]["cores"] = int(hosts[host]["cores"])
    f.close()

    return hosts

def readCmdlines(cmdlinesFile):
    cmdlines = { }
    f = open(_getCmdlinesPath(cmdlinesFile), "r")
    for line in f:
        if line.startswith("#"):
            continue
        name, cmdline = line.split(":")
        cmdlines[name] = cmdline
    f.close()

    return cmdlines

def readExp(exp):
    schedule = []
    f = open(_getExpPath(exp), "r")
    for line in f:
        if line.startswith("#"):
            continue
        tokens = line.split()
        if not len(tokens):
            continue
        stage = []
        index = 0
        currentCmdline, targetCount, currentCount = None, None, None
        allocation = None
        while index < len(tokens):
            if currentCmdline is None or targetCount == currentCount:
                if currentCmdline is not None:
                    stage.append((currentCmdline, targetCount, allocation))
                currentCmdline = tokens[index]
                targetCount = int(tokens[index+1])
                currentCount = 0
                allocation = []
            else:
                host, count = tokens[index], int(tokens[index+1])
                allocation.append((host, count))
                currentCount += count
            index += 2

        assert targetCount and targetCount == currentCount
        stage.append((currentCmdline, targetCount, allocation))
        schedule.append(stage)
                
    f.close()

    return schedule

def readKleeCmd(kleeCmd):
    f = open(_getKleeCmdPath(kleeCmd), "r")
    cmds = f.read().split()
    f.close()

    return cmds

