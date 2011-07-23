#!/usr/bin/ipython -pylab

import sys
from data import Experiment
from itertools import izip
from scipy import *
from scipy import optimize

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

def compute_instructions_approx_1(el, eldup, cuttime=50, upcuttime=None, deg=1):
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

    #eldup.a['InstructionsApprox'] = eldup.a.InstructionsTop / el.pratio
    #eldup.stats['InstructionsApprox'] = list(eldup.a.InstructionsApprox)

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

def compute_instructions_approx_2(el, eldup):
    compute_instructions_top(el)
    compute_instructions_top(eldup)

    fitfunc = lambda p, x: p[0]*log(p[1]*x+1)
    errfunc = lambda p, x, y: fitfunc(p, x) - y

    p0 = [1500000, 0.00000001] # XXX

    p1, ok = optimize.leastsq(errfunc, p0[:],
            args=(eldup.a.InstructionsTop, eldup.a.InstructionsMultExact))

    if not ok:
        print 'WARNING: fitting unsuccessful'

    eldup.a['InstructionsApprox'] = fitfunc(p1, eldup.a.InstructionsTop)
    eldup.stats['InstructionsApprox'] = list(eldup.a.InstructionsApprox)

    el.a['InstructionsApprox'] = fitfunc(p1, el.a.InstructionsTop)
    el.stats['InstructionsApprox'] = list(el.a.InstructionsApprox)

def compute_corrected_time(el, eldup):
    maxpaths = eldup.a.InstructionsMult[-1]
    idx = where(el.a.InstructionsMult > maxpaths)[0][0]
    eldup.a['ExecutionTimeCorr'] = \
        eldup.a.ExecutionTime * ( \
            el.a.ExecutionTime[idx] / eldup.a.ExecutionTime[-1])
    eldup.stats['ExecutionTimeCorr'] = list(eldup.a.ExecutionTimeCorr)

def plot_pc_time(el, eldup, elv, compute=True):
    if compute:
        compute_instructions_approx_2(el, eldup)
    eplot((el, 'InstructionsApprox'), (eldup, 'InstructionsMultExact'), (elv, 'Instructions'))

def output_files_all(exps=None, compute=True, order=None):
    if exps is None:
        exps = el

    emap = {}
    for e in exps:
        tool, kind = e.name.split('-', 1)
        l = emap.setdefault(tool, [None,None,None])
        if kind == 'lazy-merge':
            l[0] = e
        elif kind == 'duplicates':
            l[1] = e
        elif kind == 'vanilla':
            l[2] = e
        else:
            assert False, 'Unknown experiment kind %s' % kind

    elist = []

    if order is not None:
        for x in order:
            if x in emap:
                elist.append((x, emap[x]))
                del emap[x]

    for k,v in emap.iteritems():
        elist.append((k, v))

    for tool,v in elist:
        assert v[0] is not None
        assert v[1] is not None
        assert v[2] is not None
        if compute:
            compute_instructions_approx_2(v[0], v[1])

    fapprox = open('output/paths-vs-klee-approx.txt', 'w')
    fexact = open('output/paths-vs-klee-exact.txt', 'w')
    fmult = open('output/paths-mult.txt', 'w')

    for tool,v in elist:
        # Approx
        fapprox.write('time %s-sidekick\n' % tool)
        for t,p in izip(v[0].a.ExecutionTime, v[0].a.InstructionsApprox):
            fapprox.write('%f %f\n' % (t, p))
        fapprox.write('\n\n')

        fapprox.write('time %s-klee\n' % tool)
        for t,p in izip(v[2].a.ExecutionTime, v[2].a.Instructions):
            fapprox.write('%f %f\n' % (t, p))
        fapprox.write('\n\n')

        # Exact
        fexact.write('time %s-sidekick-approx %s-sidekick-exact\n' % (tool, tool))
        for t,p1,p2 in izip(v[1].a.ExecutionTime, v[1].a.InstructionsApprox, v[1].a.InstructionsMultExact):
            fexact.write('%f %f %f\n' % (t, p1, p2))
        fexact.write('\n\n')

        fexact.write('time %s-klee\n' % tool)
        for t,p in izip(v[2].a.ExecutionTime, v[2].a.Instructions):
            fexact.write('%f %f\n' % (t, p))
        fexact.write('\n\n')

        # Mult-vs-exact
        fmult.write('%s-inst-mult %s-inst-exact %s-inst-estimate\n')
        for t,p,e in izip(v[1].a.InstructionsTop, v[1].a.InstructionsMultExact,
                          v[1].a.InstructionsApprox):
            fmult.write('%f %f %f\n' % (t, p, e))
        fmult.write('\n\n')

    fapprox.close()
    fexact.close()
    fmult.close()

def output_files(el, eldup, elv, name, compute=True):
    if compute:
        compute_instructions_approx_2(el, eldup)

    f = open('paths-vs-klee-%s.txt' % name, 'w')

    f.write('Time Sidekick-Approx\n')
    for t,p in izip(el.a.ExecutionTime, el.a.InstructionsApprox):
        f.write('%f %f\n' % (t, p))

    f.write('\n\n')
    f.write('Time KLEE\n')
    for t,p in izip(elv.a.ExecutionTime, elv.a.Instructions):
        f.write('%f %f\n' % (t, p))

    f.close()

    f = open('paths-vs-klee-exact-%s.txt' % name, 'w')
    f.write('Time Sidekick-Exact\n')
    for t,p in izip(eldup.a.ExecutionTime, eldup.a.InstructionsMultExact):
        f.write('%f %f\n' % (t, p))

    f.write('\n\n')
    f.write('Time Sidekick-Approx\n')
    for t,p in izip(eldup.a.ExecutionTime, eldup.a.InstructionsApprox):
        f.write('%f %f\n' % (t, p))

    try:
        maxtime_idx = where(elv.a.ExecutionTime > 2*eldup.a.ExecutionTime[-1])[0][0]
    except IndexError:
        maxtime_idx = -1

    f.write('\n\n')
    f.write('Time KLEE\n')
    for t,p in izip(elv.a.ExecutionTime[:maxtime_idx], elv.a.Instructions[:maxtime_idx]):
        f.write('%f %f\n' % (t, p))
    f.close()

    f = open('paths-multiplicity-%s.txt' % name, 'w')
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

def compute_diffsym(exps=None):
    if exps is None:
        exps = el

    r = {}

    for exp in exps:
        if exp.is_done:
            ns = exp.name.split('-')
            exp.tool_name = ns[0]
            exp.exp_tag = ns[1]
            exp.sym_name = ns[2]
            exp.sym_num = int(ns[3])
            exp.use_lazy_merge = exp.opts.get('use_lazy_merge', '0') == '1'
            exp.use_nl = exp.opts.get('force_inline', '1') == '0'

            r.setdefault(exp.tool_name, dict()) \
                .setdefault(exp.sym_name, dict())[exp.exp_tag] = exp

    for tool_name in r.keys():
        rl = []
        for sym_name, d in r[tool_name].iteritems():
            exec_time = {}
            for tag in ('vanilla', 'lazy_merge', 'lazy_merge_nl'):
                if tag in d:
                    sym_num = d[tag].sym_num
                    exec_time[tag] = d[tag].stats.ExecutionTime[-1]
                else:
                    exec_time[tag] = 3600

            rl.append((sym_num, sym_name, exec_time['vanilla'], exec_time['lazy_merge'], exec_time['lazy_merge_nl'],
                                    exec_time['vanilla'] / min(exec_time['lazy_merge'], exec_time['lazy_merge_nl'])))
        rl.sort()
        r[tool_name] = rl

    return r

def print_diffsym(r):
    for tool, rl in r.iteritems():
        print tool + ':'
        for x in rl:
            print '  %2d (%10s) %8.2f %8.2f %8.2f ' % x[:-1],
            if x[3] != 3600 or x[4] != 3600:
                print '%8.2f' % x[5]
            else:
                print

if __name__ == '__main__':
    
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: %s klee-out-1 klee-out-2 ...\n" % sys.argv[0])
        sys.exit(1)

    el = []
    es = []

    n = 0
    for klee_dir in sys.argv[1:]:
         e = Experiment(klee_dir)
         if not hasattr(e, 'stats'):
             continue
         el.append(e)
         es.append(el[n].stats)
         globals()['el' + str(n)] = el[n]
         globals()['es' + str(n)] = el[n].stats
         n = n + 1


#    from IPython.Shell import IPShellEmbed
#    ipshell = IPShellEmbed()
#    ipshell()
