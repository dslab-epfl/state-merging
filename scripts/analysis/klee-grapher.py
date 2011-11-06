#!/usr/bin/python

import sys
import pickle
import socket
from data import Experiment, ClassDict
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
        update_experiments(opts['exps'])

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
    """ Plot GloballyCoveredInstructions versus ExecutionTime graph for given exps """
    if exps is None:
        exps = el

    clf()
    eplot('GloballyCoveredInstructions', update=update, exps=exps)

def compute_instructions_approx_1(el, eldup, cuttime=50, upcuttime=None, deg=1):
    cut_el = where(el.stats.ExecutionTime > cuttime)[0][0]
    cut_eldup = where(eldup.stats.ExecutionTime > cuttime)[0][0]

    el.pfit = polyfit(eldup.stats.ExecutionTime[cut_eldup:],
            eldup.stats.InstructionsMultApprox[cut_eldup:] /
                eldup.stats.InstructionsMultExact[cut_eldup:],
            deg)

    el.pratio = polyval(el.pfit, el.stats.ExecutionTime)
    el.pratio = where(el.stats.ExecutionTime < cuttime, el.pratio, el.pratio[cut_el])
    if upcuttime is not None:
        upcut_el = where(el.stats.ExecutionTime > upcuttime)[0][0]
        el.pratio = where(el.stats.ExecutionTime < upcuttime, el.pratio, el.pratio[upcut_el])

    el.stats['InstructionsApprox'] = el.stats.InstructionsMultApprox / el.pratio

    #eldup.stats['InstructionsApprox'] = eldup.stats.InstructionsMultApprox / el.pratio

def compute_instructions_approx(el, eldup, deg=1):
    el.pfit = polyfit(eldup.stats.InstructionsMultApprox,
                      eldup.stats.InstructionsMultExact,
                      deg)

    el.stats['InstructionsApprox'] = polyval(el.pfit, el.stats.InstructionsMultApprox)

    eldup.stats['InstructionsApprox'] = polyval(el.pfit, eldup.stats.InstructionsMultApprox)

def compute_instructions_approx_2(el, eldup):
    fitfunc = lambda p, x: p[0]*log(p[1]*x+1)
    errfunc = lambda p, x, y: fitfunc(p, x) - y

    p0 = [1500000, 0.00000001] # XXX

    p1, ok = optimize.leastsq(errfunc, p0[:],
            args=(eldup.stats.InstructionsMultApprox, eldup.stats.InstructionsMultExact))

    if not ok:
        print 'WARNING: fitting unsuccessful'

    eldup.stats['InstructionsApprox'] = fitfunc(p1, eldup.stats.InstructionsMultApprox)

    el.stats['InstructionsApprox'] = fitfunc(p1, el.stats.InstructionsMultApprox)

def compute_corrected_time(el, eldup):
    maxpaths = eldup.stats.InstructionsMult[-1]
    idx = where(el.stats.InstructionsMult > maxpaths)[0][0]
    eldup.stats['ExecutionTimeCorr'] = \
        eldup.stats.ExecutionTime * ( \
            el.stats.ExecutionTime[idx] / eldup.stats.ExecutionTime[-1])
    eldup.stats['ExecutionTimeCorr'] = list(eldup.stats.ExecutionTimeCorr)

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
        for t,p in izip(v[0].stats.ExecutionTime, v[0].stats.InstructionsApprox):
            fapprox.write('%f %f\n' % (t, p))
        fapprox.write('\n\n')

        fapprox.write('time %s-klee\n' % tool)
        for t,p in izip(v[2].stats.ExecutionTime, v[2].stats.Instructions):
            fapprox.write('%f %f\n' % (t, p))
        fapprox.write('\n\n')

        # Exact
        fexact.write('time %s-sidekick-approx %s-sidekick-exact\n' % (tool, tool))
        for t,p1,p2 in izip(v[1].stats.ExecutionTime, v[1].stats.InstructionsApprox, v[1].stats.InstructionsMultExact):
            fexact.write('%f %f %f\n' % (t, p1, p2))
        fexact.write('\n\n')

        fexact.write('time %s-klee\n' % tool)
        for t,p in izip(v[2].stats.ExecutionTime, v[2].stats.Instructions):
            fexact.write('%f %f\n' % (t, p))
        fexact.write('\n\n')

        # Mult-vs-exact
        fmult.write('%s-inst-mult %s-inst-exact %s-inst-estimate\n')
        for t,p,e in izip(v[1].stats.InstructionsMultApprox, v[1].stats.InstructionsMultExact,
                          v[1].stats.InstructionsApprox):
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
    for t,p in izip(el.stats.ExecutionTime, el.stats.InstructionsApprox):
        f.write('%f %f\n' % (t, p))

    f.write('\n\n')
    f.write('Time KLEE\n')
    for t,p in izip(elv.stats.ExecutionTime, elv.stats.Instructions):
        f.write('%f %f\n' % (t, p))

    f.close()

    f = open('paths-vs-klee-exact-%s.txt' % name, 'w')
    f.write('Time Sidekick-Exact\n')
    for t,p in izip(eldup.stats.ExecutionTime, eldup.stats.InstructionsMultExact):
        f.write('%f %f\n' % (t, p))

    f.write('\n\n')
    f.write('Time Sidekick-Approx\n')
    for t,p in izip(eldup.stats.ExecutionTime, eldup.stats.InstructionsApprox):
        f.write('%f %f\n' % (t, p))

    try:
        maxtime_idx = where(elv.stats.ExecutionTime > 2*eldup.stats.ExecutionTime[-1])[0][0]
    except IndexError:
        maxtime_idx = -1

    f.write('\n\n')
    f.write('Time KLEE\n')
    for t,p in izip(elv.stats.ExecutionTime[:maxtime_idx], elv.stats.Instructions[:maxtime_idx]):
        f.write('%f %f\n' % (t, p))
    f.close()

    f = open('paths-multiplicity-%s.txt' % name, 'w')
    f.write('MultInstructions ExactInstructions ApproxInstructions\n')
    for t,p,e in izip(eldup.stats.InstructionsMult, eldup.stats.InstructionsMultExact,
                      eldup.stats.InstructionsApprox):
        f.write('%f %f %f\n' % (t,p,e))
    f.close()

def update_experiments(exps=None):
    if exps is None:
        exps = el

    for exp in exps:
        exp.update()

def _compute_diffsym(exps=None):
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
            vanilla_is_done = False
            lsm_is_done = False
            for tag in ('vanilla', 'lazy_merge', 'lazy_merge_nl', 'lazy_merge_t', 'lazy_merge_nl_t'):
                if tag in d:
                    sym_num = d[tag].sym_num
                    exec_time[tag] = d[tag].stats.ExecutionTime[-1]
                    #exec_time[tag] = d[tag].stats.UserTime[-1]
                    if d[tag].is_done:
                        if tag == 'vanilla':
                            vanilla_is_done = True
                        else:
                            lsm_is_done = True
                else:
                    exec_time[tag] = 3600*2

            if not lsm_is_done:
                continue

            l = [sym_num, sym_name, exec_time['vanilla'], exec_time['lazy_merge'], exec_time['lazy_merge_nl'],
                    exec_time['lazy_merge_t'], exec_time['lazy_merge_nl_t']]
            m = min(l[3:])
            l.append(m)
            l.append(exec_time['vanilla'] / m)
            rl.append(l)
            #rl.append((sym_num, sym_name, exec_time['vanilla'], exec_time['lazy_merge'], exec_time['lazy_merge_nl'],
            #                        exec_time['vanilla'] / min(exec_time['lazy_merge'], exec_time['lazy_merge_nl'])))

        rl.sort()
        r[tool_name] = rl

    return r

def compute_diffsym0(exps=None):
    r = _compute_diffsym(exps)
    for k in r.keys():
        for i in xrange(len(r[k])):
            l = r[k][i]
            r[k][i] = l[:3] + l[-2:]
    return r

def categorize_exps(exps=None):
    if exps is None:
        exps = el

    ex = ClassDict()
    for e in exps:
        exI = ex
        name_parts = e.name.split('-')

        # XXX
        if len(name_parts) > 3:
            name_parts = [name_parts[0], 'n'+name_parts[3], name_parts[1]]

        for part in name_parts[:-1]:
            if exI.has_key(part):
                if not isinstance(exI[part], ClassDict):
                    exInew = ClassDict()
                    exInew[None] = exI[part]
                    exI[part] = exInew
            else:
                exI[part] = ClassDict()
            exI = exI[part]

        part = name_parts[-1]
        if exI.has_key(part):
            assert isinstance(exI[part], ClassDict())
            exI[part][None] = e
        else:
            exI[part] = e

    return ex

def compute_diffsym(exps=None, timeout=3600.0):
    if exps is None:
        exps = el

    result = {}
    exps = categorize_exps(exps)
    for tool, tool_exps in exps.iteritems():
        for exp_n, exp in tool_exps.iteritems():
            lm_finished = klee_finished = False
            lm_time = klee_time = timeout # XXX

            if exp.has_key('lazy_merge'):
                lm_finished = exp.lazy_merge.is_done
                lm_time = exp.lazy_merge.stats.ExecutionTime[-1]

            if exp.has_key('lazy_merge_nl'):
                lm_finished = lm_finished or exp.lazy_merge_nl.is_done
                lm_time = min((lm_time, exp.lazy_merge_nl.stats.ExecutionTime[-1]))

            if exp.has_key('vanilla'):
                klee_finished = exp.vanilla.is_done
                # We are re-running vanilla experiments right now... hack to skip non-finished re-runs
                if klee_finished or exp.vanilla.stats.ExecutionTime[-1] > timeout:
                    klee_time = exp.vanilla.stats.ExecutionTime[-1]

            if lm_finished:
                result.setdefault(tool, list()).append([
                    int(exp_n[1:]), exp_n, klee_time, lm_time, klee_time / lm_time, klee_finished])

            # Some interesting cases:
            if klee_finished and not lm_finished:
                print "klee_finished and not lm_finished:", tool, exp_n

    for k in result.keys():
        result[k].sort(cmp=lambda x, y: cmp(x[0], y[0]))

    return result

def compute_time(exps=None):
    if exps is None:
        exps = el

    exps_c = 'vanilla lazy static static_nfl'.split()

    result = { None: [['n'] + exps_c] }
    exps = categorize_exps(exps)
    for tool, tool_exps in exps.iteritems():
        result_tool = []
        for exp_n, exp in tool_exps.iteritems():
            result_exp = [exp_n]
            for c in exps_c:
                if exp.has_key(c) and 'WallTime' in exp[c].stats:
                    fmt = '%.0f'
                    if not exp[c].is_done:
                        fmt = '>=' + fmt
                    result_exp.append(fmt % (exp[c].stats.WallTime[-1]))
                else:
                    result_exp.append('')
            result_tool.append(result_exp)
        result_tool.sort()
        result[tool] = result_tool

    return result

def print_time(t):
    print ' '*8, ' '.join(['%8s' % c for c in t[None][0]])

    for tool, tool_exps in t.iteritems():
        if tool is None:
            continue
        print '%8s' % tool
        for exp in tool_exps:
            print ' '*8, ' '.join(['%8s' % c for c in exp])

def get_stat_at(e, stat, t):
    idx = where(e.stats.WallTime >= t)[0][:1]
    if idx:
        return e.stats[stat][idx[0]]
    else:
        return 0

def compute_lcov(exps=None):
    if exps is None:
        exps = el

    exps_c = 'vanilla lazy static'.split()

    result = { None: exps_c }
    exps = categorize_exps(exps)
    for tool, exp in exps.iteritems():
        min_time = 3600
        for c in exps_c:
            if exp.has_key(c) and 'WallTime' in exp[c].stats:
                t = exp[c].stats.WallTime[-1]
                if t < min_time:
                    min_time = t

        result_tool = []
        for c in exps_c:
            if exp.has_key(c) and 'WallTime' in exp[c].stats:
                result_tool.append(int(get_stat_at(exp[c],
                    'GloballyCoveredInstructions', min_time)))
            else:
                result_tool.append(0)

        for c in exps_c:
            if exp.has_key(c) and 'WallTime' in exp[c].stats:
                total_insts = exp[c].stats.GloballyCoveredInstructions[0] + \
                              exp[c].stats.GloballyUncoveredInstructions[0]
                pct = float(get_stat_at(exp[c], 'GloballyCoveredInstructions',
                                min_time)) / total_insts
                if exp[c].is_done:
                    result_tool.append('* %.2f' % pct)
                else:
                    result_tool.append('  %.2f' % pct)
            else:
                result_tool.append('')

        result[tool] = result_tool

    return result

def print_lcov(l):
    print ' '*8, ' '.join(['%8s' % c for c in l[None]])

    for tool, exp in l.iteritems():
        if tool is None:
            continue
        print '%8s' % tool, ' '.join(['%8s' % c for c in exp])

def print_diffsym(r):
    for tool, rl in r.iteritems():
        print tool + ':'
        for x in rl:
            print '  %2d (%10s) %8.2f %8.2f' % \
                    (x[0], x[1], x[2], x[3]),
            if x[3] != 3600*2:
                if x[2] == 3600*2:
                    print ' >%8.2f' % x[4]
                else:
                    print ' %8.2f' % x[4]
            else:
                print

def _print_diffsym(r):
    for tool, rl in r.iteritems():
        print tool + ':'
        for x in rl:
            print '  %2d (%10s) %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f' % tuple(x[:-1]),
            if x[-2] != 3600*2:
                if x[2] == 3600*2:
                    print ' >%8.2f' % x[-1]
                else:
                    print ' %8.2f' % x[-1]
            else:
                print

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='KLEE data analyzer')
    #parser.add_argument('-i', '--interactive', action='store_true', help='Run in interactive mode')
    parser.add_argument('--incomplete', action='store_true', help='Include incomplete experiments')
    parser.add_argument('--timeout', type=int, default=0,
                        help='Timeout in seconds after which an experiment is considered completed')
    parser.add_argument('--diffsym', action='store_true', help='Compute and store diffsym results')
    parser.add_argument('--time', action='store_true', help='Compute time for results')
    parser.add_argument('--lcov', action='store_true', help='Compute lcov for results')
    parser.add_argument('experiments', nargs='+', help='Experiments to analyze')

    args = parser.parse_args()

    el = []             # A list of all experiments
    ex = ClassDict()    # A tree of all experiments sorted by dash-separated parts in the names

    n = 0
    for klee_dir in args.experiments:
        e = Experiment(klee_dir)

        if not e.is_loaded:
            print 'Cannot load experiment: ' + klee_dir
            continue

        if not args.incomplete:
            if not e.is_done:
                if not args.timeout or e.stats.WallTime[-1] < args.timeout:
                    print 'Experiment is incomplete: ' + klee_dir
                    continue

        el.append(e)
        globals()['el' + str(n)] = el[n]
        n = n + 1

    print 'Loaded %d experiments' % (n,)

    ex = categorize_exps(el)

    if args.diffsym:
        print 'Computing diffsym data...'
        r = compute_diffsym(timeout)
        pickle.dump(r, open('diffsym.pickle', 'w'))
        print_diffsym(r)

    if args.time:
        t = compute_time(el)
        pickle.dump(t, open('diffsym.%s.pickle' % socket.gethostname(), 'w'))
        print_time(t)

    if args.lcov:
        l = compute_lcov(el)
        pickle.dump(l, open('lcov.%s.pickle' % socket.gethostname(), 'w'))
        print_lcov(l)

    #if not args.interactive:
    #    sys.exit(0)

#    from IPython.Shell import IPShellEmbed
#    ipshell = IPShellEmbed()
#    ipshell()


