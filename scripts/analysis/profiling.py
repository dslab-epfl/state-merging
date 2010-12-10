"""
Cloud9 profiling
"""

import c9instrum

from c9instrum import Events
from c9instrum import Statistics
from c9instrum import EventAttributes

import re
import math

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

def gen_multiplicity_distribution(outFile, eventEntries):
    relevantEvents = filter(lambda event: event.id == Events.CONSTRAINT_SOLVE, eventEntries)

    for event in relevantEvents:        
        outFile.write("%d %d %.3f\n" % (
                int(event.values.get(EventAttributes.STATE_DEPTH)),
                int(event.values.get(EventAttributes.STATE_MULTIPLICITY)),
                float(event.values.get(EventAttributes.THREAD_TIME))
                ))

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
