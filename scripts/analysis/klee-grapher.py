#!/usr/bin/ipython -pylab

import sys
from data import Experiment
from itertools import izip
from scipy import *

def eplot(*args, **kwargs):
    opts = {
            'exps': [],
            'x': 'ExecutionTime',
            'update': False,
            'logx': False,
            'logy': False,
            }

    for k,v in opts.iteritems():
        if k in kwargs:
            opts[k] = kwargs[k]
            del kwargs[k]

    if opts['update']:
        update_experiments()

    func = plot
    if opts['logx']:
        if opts['logy']:
            func = loglog
        else:
            func = semilogx
    elif opts['logy']:
        func = semilogy

    for arg in args:
        if isinstance(arg, basestring):
            for exp in opts['exps']:
                if arg in exp.stats:
                    func(exp.stats[opts['x']], exp[arg], label=exp.name + ':' + arg, **kwargs)
        else:
            exp, param = arg
            func(exp.stats[opts['x']], exp[param], label=exp.name + ':' + param, **kwargs)

    rcParams.update({'legend.fontsize': 10})
    legend(loc='lower right')

def plot_cov_time(update=True, exps=None):
    if exps is None:
        exps = el

    clf()
    eplot('GloballyCoveredInstructions', update=update, exps=exps)

def compute_instructions_top(el):
    el.a['InstructionsTop'] = el.a.InstructionsMult + el.a.InstructionsMultHigh * float(2**64)
    el.stats['InstructionsTop'] = list(el.a.InstructionsTop)

"""
def compute_instructions_approx(el, eldup, cuttime=10, upcuttime=None, deg=1):
    compute_instructions_top(el)
    compute_instructions_top(eldup)
    
    cut_el = where(el.a.ExecutionTime > cuttime)[0][0]
    cut_eldup = where(eldup.a.ExecutionTime > cuttime)[0][0]

    el.pfit = polyfit(eldup.a.ExecutionTime[cut_eldup:],
            eldup.a.InstructionsTop[cut_eldup:] /
                eldup.a.InstructionsMultExact[cut_eldup:],
            deg)

    el.pratio = polyval(el.pfit, el.a.ExecutionTime)
    el.pratio = where(el.a.ExecutionTime < cuttime, el.pratio, el.pratio[cut_el])
    if upcuttime is not None:
        upcut_el = where(el.a.ExecutionTime > upcuttime)[0][0]
        el.pratio = where(el.a.ExecutionTime < upcuttime, el.pratio, el.pratio[upcut_el])

    el.a['InstructionsApprox'] = el.a.InstructionsTop / el.pratio
    el.stats['InstructionsApprox'] = list(el.a.InstructionsApprox)
"""

def compute_instructions_approx(el, eldup, deg=1):
    compute_instructions_top(el)
    compute_instructions_top(eldup)
    
    el.pfit = polyfit(eldup.a.InstructionsTop,
                      eldup.a.InstructionsMultExact,
                      deg)

    el.a['InstructionsApprox'] = polyval(el.pfit, el.a.InstructionsTop)
    el.stats['InstructionsApprox'] = list(el.a.InstructionsApprox)

    eldup.a['InstructionsApprox'] = polyval(el.pfit, eldup.a.InstructionsTop)
    eldup.stats['InstructionsApprox'] = list(eldup.a.InstructionsApprox)

def compute_corrected_time(el, eldup):
    maxpaths = eldup.a.InstructionsMult[-1]
    idx = where(el.a.InstructionsMult > maxpaths)[0][0]
    eldup.a['ExecutionTimeCorr'] = \
        eldup.a.ExecutionTime * ( \
            el.a.ExecutionTime[idx] / eldup.a.ExecutionTime[-1])
    eldup.stats['ExecutionTimeCorr'] = list(eldup.a.ExecutionTimeCorr)

def plot_pc_time(el, eldup, elv, compute=True):
    compute_instructions_approx(el, eldup)
    eplot((el, 'InstructionsApprox'), (eldup, 'InstructionsMultExact'), (elv, 'Instructions'))

def output_files(el, eldup, elv, compute=True):
    compute_instructions_approx(el, eldup)

    f = open('paths-vs-klee.txt', 'w')

    f.write('Time Sidekick-Approx\n')
    for t,p in izip(el.a.ExecutionTime, el.a.InstructionsApprox):
        f.write('%f %f\n' % (t, p))

    f.write('\n\n')
    f.write('Time KLEE\n')
    for t,p in izip(elv.a.ExecutionTime, elv.a.Instructions):
        f.write('%f %f\n' % (t, p))

    f.close()

    f = open('paths-vs-klee-exact.txt', 'w')
    f.write('Time Sidekick-Exact\n')
    for t,p in izip(eldup.a.ExecutionTime, eldup.a.InstructionsMultExact):
        f.write('%f %f\n' % (t, p))

    f.write('\n\n')
    f.write('Time Sidekick-Approx\n')
    for t,p in izip(eldup.a.ExecutionTime, eldup.a.InstructionsApprox):
        f.write('%f %f\n' % (t, p))

    maxtime_idx = where(elv.a.ExecutionTime > 2*eldup.a.ExecutionTime[-1])[0][0]

    f.write('\n\n')
    f.write('Time KLEE\n')
    for t,p in izip(elv.a.ExecutionTime[:maxtime_idx], elv.a.Instructions[:maxtime_idx]):
        f.write('%f %f\n' % (t, p))
    f.close()

    f = open('paths-multiplicity.txt', 'w')
    f.write('MultInstructions ExactInstructions ApproxInstructions\n')
    for t,p,e in izip(eldup.a.InstructionsMult, eldup.a.InstructionsMultExact,
                      eldup.a.InstructionsApprox):
        f.write('%f %f %f\n' % (t,p,e))
    f.close()

def update_experiments(exps=None):
    if exps is None:
        exps = el

    for exp in exps:
        exp.update()

if __name__ == '__main__':
    
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: %s klee-out-1 klee-out-2 ...\n" % sys.argv[0])
        sys.exit(1)

    el = []
    es = []
    for n, klee_dir in enumerate(sys.argv[1:]):
         el.append(Experiment(klee_dir))
         es.append(el[n].stats)
         globals()['el' + str(n)] = el[n]
         globals()['es' + str(n)] = el[n].stats


#    from IPython.Shell import IPShellEmbed
#    ipshell = IPShellEmbed()
#    ipshell()
