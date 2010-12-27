"""
Cloud9 profiling
"""

import c9instrum

from c9instrum import Events
from c9instrum import Statistics
from c9instrum import EventAttributes

import re
import math
import sys

class ProfilingExperiment:
    def __init__(self):
        self.workerCount = None
        self.wTimelines = []
        self.lbTimeline = None

def parse_klee_dir(dirName):
    statEntries = c9instrum.parse_stats(dirName + "/c9-stats.txt")
    eventEntries = c9instrum.parse_events(dirName + "/c9-events.txt")

    return statEntries, eventEntries

def parse_experiment(dirName, workerCount):
    result = ProfilingExperiment()
    result.workerCount = workerCount
    
    for i in range(1, workerCount+1):
        statEntries, eventEntries = parse_klee_dir(dirName + ("/worker%d" % i))
    result.wTimelines.append( (statEntries, eventEntries) )
    
    return result

def gen_stp_distributions(outFile, experiment):
    smtBuckets = { }
    smtTotal = 0.0
    
    satBuckets = { }
    satTotal = 0.0
    
    for timeline in experiment.wTimelines:
        invokeCount = 0
    lastTimeStamp = 0.0
    
    for event in timeline[1]:
        assert event.timeStamp >= lastTimeStamp
        
        if event.id == Events.SMT_SOLVE or event.id == Events.SAT_SOLVE:
            timing, _ = c9instrum.parse_timer(event)
        
        bucket = int(math.floor(math.log10(timing[0])))
        
        if event.id == Events.SMT_SOLVE:
            oldVal = smtBuckets.get(bucket, (0, 0, 0))
            smtBuckets[bucket] = (oldVal[0] + 1, oldVal[1] + timing[0], oldVal[2] + invokeCount)
            smtTotal += timing[0]
            
            invokeCount = 0
        else:
            oldVal = satBuckets.get(bucket, (0, 0))
            satBuckets[bucket] = (oldVal[0] + 1, oldVal[1] + timing[0])
            satTotal += timing[0]
            
            invokeCount = invokeCount + 1
            
            lastTimeStamp = event.timeStamp
            
            minimum = min(smtBuckets.keys() + satBuckets.keys())
            maximum = max(smtBuckets.keys() + satBuckets.keys())
            
            for data in ((smtBuckets, smtTotal), (satBuckets, satTotal)):
                
                for i in range(minimum, maximum+2):
                    values = data[0].get(i, (0, 0, 0))
                    if values[0] == 0:
                        outFile.write("10^{%d} 0 0\n" % i)
        continue
    
    if len(values) == 2:
        outFile.write("10^{%d} %d %.3f\n" % (i, values[0], values[1]*100.0/data[1]))
    else:
        outFile.write("10^{%d} %d %.3f %.3f\n" % (i, values[0], values[1]*100.0/data[1], float(values[2])/values[0]))
        
    outFile.write("\n\n")

def _compute_histogram(dataSet, keyFunc, valueFunc):
    """
    Computes a histogram data structure based on a set of event data.
    """

    histData = { }
    for entry in dataSet:
        key, value = keyFunc(entry), valueFunc(entry)

        if key not in histData:
            histData[key] = []

        histData[key].append(value)

    histogram = { }
    for (key, values) in histData.iteritems():
        avg = sum(values) / len(values)
        stddev = math.sqrt(sum([(x - avg)*(x - avg) for x in values])/
                           (len(values) - 1)) \
                           if len(values) > 1 else 0
    
        histogram[key] = (min(values), max(values), avg, stddev)

    return histogram

def _write_histogram(outFile, histogram, labelFunc):
    keys = histogram.keys()

    for key in range(0, max(keys) + 1):
        value = histogram.get(key, (0.0, 0.0, 0.0, 0.0))
        outFile.write("%d %.3f %.3f %.3f %.3f\n" % (labelFunc(key), value[0], value[1], value[2], value[3]))


def gen_multiplicity_distribution(outFile, eventEntries):
    csEvents = filter(lambda event: event.id == Events.CONSTRAINT_SOLVE and 
                      event.values.get(EventAttributes.STATE_DEPTH) and
                      event.values.get(EventAttributes.STATE_MULTIPLICITY), eventEntries)
    stpEvents = filter(lambda event: event.id == Events.SMT_SOLVE and
                       event.values.get(EventAttributes.STATE_DEPTH) and
                       event.values.get(EventAttributes.STATE_MULTIPLICITY), eventEntries)

    outFile.write("#"*80 + "\n")
    outFile.write("# [Index 0] Constraint solving events (Depth, Multiplicity, Wall Time)\n")
    for event in csEvents:        
        outFile.write("%d %d %.3f\n" % (
                int(event.values.get(EventAttributes.STATE_DEPTH)),
                int(event.values.get(EventAttributes.STATE_MULTIPLICITY)),
                float(event.values.get(EventAttributes.WALL_TIME))
                ))

    outFile.write("\n\n")

    stpSATEvents = filter(lambda event: event.values.get(EventAttributes.SOLVING_RESULT) == "1", stpEvents)
    stpUNSATEvents = filter(lambda event: event.values.get(EventAttributes.SOLVING_RESULT) == "0", stpEvents)

    outFile.write("#"*80 + "\n")
    outFile.write("# [Index 1 & 2] STP solving events (Depth, Multiplicity, Wall Time) for SAT and UNSAT\n")
    for eventList in (stpSATEvents, stpUNSATEvents):
        for event in eventList:
            outFile.write("%d %d %.3f\n" % (
                    int(event.values.get(EventAttributes.STATE_DEPTH)),
                    int(event.values.get(EventAttributes.STATE_MULTIPLICITY)),
                    float(event.values.get(EventAttributes.WALL_TIME))
                    ))

        outFile.write("\n\n")
        outFile.write("#"*80 + "\n")

    outFile.write("# [Index 3] STP solving time vs. depth histogram\n")
    depthHistogram = _compute_histogram(stpEvents, 
                                        lambda event: int(event.values.get(EventAttributes.STATE_DEPTH)),
                                        lambda event: float(event.values.get(EventAttributes.WALL_TIME))
                                        )

    _write_histogram(outFile, depthHistogram, lambda key: key)

    outFile.write("\n\n")
    outFile.write("#"*80 + "\n")
    outFile.write("# [Index 4] STP solving time vs. multiplicity histogram\n")

    mplicityHistogram = _compute_histogram(stpEvents,
                                           lambda event: int(math.log(int(event.values.get(EventAttributes.STATE_MULTIPLICITY)), 2)),
                                           lambda event: float(event.values.get(EventAttributes.WALL_TIME))
                                           )

    _write_histogram(outFile, mplicityHistogram, lambda key: math.pow(2, key))

    outFile.write("\n\n")
    outFile.write("#"*80 + "\n")
    outFile.write("# [Index 5] State multiplicity vs. depth histogram\n")
    
    mplicityDepthHist = _compute_histogram(stpEvents,
                                           lambda event: int(event.values.get(EventAttributes.STATE_DEPTH)),
                                           lambda event: int(event.values.get(EventAttributes.STATE_MULTIPLICITY))
                                           )
    _write_histogram(outFile, mplicityDepthHist, lambda key: key)

    outFile.write("\n\n")
    outFile.write("#"*80 + "\n")
    outFile.write("# [Index 6] Constraint solving time vs. multiplicity histogram\n")

    csHistogram = _compute_histogram(csEvents,
                                     lambda event: int(math.log(int(event.values.get(EventAttributes.STATE_MULTIPLICITY)), 2)),
                                     lambda event: float(event.values.get(EventAttributes.WALL_TIME))
                                     )
    
    _write_histogram(outFile, csHistogram, lambda key: math.pow(2, key))

def gen_worker_profile(outFile, experiment, worker, resolution):
    timeline = experiment.wTimelines[worker-1][1]
    sliceCount = int(math.ceil(timeline[-1].timeStamp / resolution))
    
    slices = [[0.0, 0.0, 0.0] for i in range(sliceCount)]
    
    for event in timeline:
        if event.id in [Events.INSTRUCTION_BATCH, Events.SMT_SOLVE, Events.SAT_SOLVE]:
            timing, _ = c9instrum.parse_timer(event.value)
            
            startTime = event.timeStamp - timing[1]
            endTime = event.timeStamp
            
            startSlice = int(math.floor(startTime / resolution))
            endSlice = int(math.floor(endTime / resolution))
            
            if event.id == Events.INSTRUCTION_BATCH:
                slicePos = 0
            elif event.id == Events.SMT_SOLVE:
                slicePos = 1
            elif event.id == Events.SAT_SOLVE:
                slicePos = 2
                
                assert startSlice <= endSlice
                
                if startSlice == endSlice:
                    slices[startSlice][slicePos] += (endTime - startTime)
                else:
                    slices[startSlice][slicePos] += (startSlice + 1) * resolution - startTime
        slices[endSlice][slicePos] += endTime - endSlice * resolution
        
        if endSlice - startSlice > 1:
            for i in range(startSlice+1, endSlice):
                slices[i][slicePos] += resolution
    
    
                for i in range(len(slices)):
                    outFile.write("%.3f " % (i * resolution))
    
    for time in slices[i]:
        outFile.write("%.3f " % (time*100.0/resolution))
    outFile.write("\n")
