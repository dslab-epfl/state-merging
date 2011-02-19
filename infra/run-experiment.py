#!/usr/bin/env python

import sys

from expmanager import ExperimentManager
from argparse import ArgumentParser

def main():
    parser = ArgumentParser(description="Run Cloud9 experiments.",
                            fromfile_prefix_chars="@")
    parser.add_argument("hosts", help="Available cluster machines")
    parser.add_argument("cmdlines", help="Command lines of the testing targets")
    parser.add_argument("exp", help="The experiment schedule file")
    parser.add_argument("kleecmd", help="The command line parameters to pass to Klee")
    parser.add_argument("coverable", help="The file containing the coverable files")
    parser.add_argument("--debugcomm", action="store_true", default=False, 
                        help="Enable worker communication debugging output (*very* verbose)")
    parser.add_argument("-p", "--prefix", default="test",
                        help="Prefix for the testing output directory")
    parser.add_argument("-t", "--duration", default=3600, type=int,
                        help="The duration of each experiment")
    parser.add_argument("--lb-stop", type=int,
                        help="The duraction of load balancing")
    parser.add_argument("--strategy", help="Worker search strategy.")
    
    args = parser.parse_args()

    manager = ExperimentManager(args.hosts, args.cmdlines, args.exp,
                                args.kleecmd, args.coverable,
                                debugcomm=args.debugcomm, uidprefix=args.prefix,
                                duration=args.duration,
                                balancetout=args.lb_stop,
                                strategy=args.strategy)
    manager.initHosts()
    manager.runExperiment()

if __name__ == "__main__":
    main()
