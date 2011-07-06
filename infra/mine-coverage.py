#!/usr/bin/env python

from argparse import ArgumentParser
from coverageminer import CoverageMiner

def main():
    parser = ArgumentParser(description="Mine Cloud9 experiments.",
                            fromfile_prefix_chars="@")
    parser.add_argument("hosts", help="Available cluster machines")
    parser.add_argument("tests", nargs="*", help="Test names")
    parser.add_argument("-f", action="append", help="File containing test names")
    parser.add_argument("-t", action="store_true", default=False, help="Display in a tabular, human-readable format")
    parser.add_argument("-m", action="append", help="Mine only the specified machines")
    parser.add_argument("-c", nargs="+", help="Measure the time it takes to get a coverage level")
    parser.add_argument("-x", action="append", help="Mine coverage only for functions listed in the specified file")

    args = parser.parse_args()
    tests = args.tests[:]

    if args.f:
        for fname in args.f:
            f = open(fname, "r")
            tests.extend(f.read().split())
            f.close()
    
    ffilter = None
    if args.x:
        for fname in args.x:
            f = open(fname, "r")
            if ffilter is None:
                ffilter = []
            ffilter.extend(f.read().split())
            f.close()

    covminer = CoverageMiner(args.hosts, 
                             hfilter=args.m, 
                             ffilter=ffilter,
                             targetcov=map(float, args.c[1:]) if args.c else None)
    covminer.analyzeExperiments(tests)

    if args.c:
        covminer.printMinTimes(args.c[0])
    else:
        covminer.printCoverageStats("human" if args.t else "internal")

if __name__ == "__main__":
    main()
