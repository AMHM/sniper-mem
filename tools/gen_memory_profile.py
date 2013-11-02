#!/usr/bin/env python

import sys, os, re, collections, subprocess, sniper_lib, sniper_config


def ex_ret(cmd):
  return subprocess.Popen(cmd, stdout = subprocess.PIPE).communicate()[0]
def cppfilt(name):
  return ex_ret([ 'c++filt', name ])


class Function:
  def __init__(self, eip, name, location):
    self.eip = eip
    self.name = cppfilt(name).strip()
    self.location = location.split(':')
    self.img = self.location[0]
    self.offset = long(self.location[1])
  def __str__(self):
    return '[%12s]  %-20s %s' % (self.eip, self.name, ':'.join(self.location))


class AllocationSite:
  def __init__(self, stack, numallocations, totalallocated, hitwhereload, hitwherestore, evictedby):
    self.stack = stack
    self.numallocations = numallocations
    self.totalallocated = totalallocated
    self.totalloads = sum(hitwhereload.values())
    self.hitwhereload = hitwhereload
    self.totalstores = sum(hitwherestore.values())
    self.hitwherestore = hitwherestore
    self.evictedby = evictedby


def format_abs_ratio(val, tot):
  if tot:
    return '%12d  (%5.1f%%)' % (val, (100. * val) / tot)
  else:
    return '%12d          ' % val


class MemoryTracker:
  def __init__(self, resultsdir = '.'):
    filename = os.path.join(resultsdir, 'sim.memorytracker')
    if not os.path.exists(filename):
      raise IOError('Cannot find output file %s' % filename)

    results = sniper_lib.get_results(resultsdir = resultsdir)
    config = results['config']
    stats = results['results']

    self.hitwhere_load_global = dict([ (k.split('-', 3)[3], sum(v)) for k, v in stats.items() if k.startswith('L1-D.loads-where-') ])
    self.hitwhere_load_unknown = self.hitwhere_load_global.copy()
    self.hitwhere_store_global = dict([ (k.split('-', 3)[3], sum(v)) for k, v in stats.items() if k.startswith('L1-D.stores-where-') ])
    self.hitwhere_store_unknown = self.hitwhere_store_global.copy()

    llc_level = int(sniper_config.get_config(config, 'perf_model/cache/levels'))
    self.evicts_global = sum([ sum(v) for k, v in stats.items() if re.match('L%d.evict-.$' % llc_level, k) ])
    self.evicts_unknown = self.evicts_global

    self.functions = {}
    self.sites = {}

    fp = open(filename)
    for line in fp:
      if line.startswith('W\t'):
        self.hitwheres = line.strip().split('\t')[1].strip(',').split(',')
      elif line.startswith('F\t'):
        _, eip, name, location = line.strip().split('\t')
        self.functions[eip] = Function(eip, name, location)
      elif line.startswith('S\t'):
        line = line.strip().split('\t')
        siteid = line[1]
        stack = line[2].strip(':').split(':')
        results = { 'numallocations': 0, 'totalallocated': 0, 'hitwhereload': {}, 'hitwherestore': {}, 'evictedby': {} }
        for data in line[3:]:
          key, value = data.split('=')
          if key == 'num-allocations':
            results['numallocations'] = long(value)
          if key == 'total-allocated':
            results['totalallocated'] = long(value)
          elif key == 'hit-where':
            entries = map(lambda s: s.split(':'), value.strip(',').split(','))
            results['hitwhereload'] = dict([ (s[1:], long(v)) for s, v in entries if s.startswith('L') ])
            for k, v in results['hitwhereload'].items():
              self.hitwhere_load_unknown[k] -= v
            results['hitwherestore'] = dict([ (s[1:], long(v)) for s, v in entries if s.startswith('S') ])
            for k, v in results['hitwherestore'].items():
              self.hitwhere_store_unknown[k] -= v
          elif key == 'evicted-by':
            results['evictedby'] = dict(map(lambda (s, v): (s, long(v)), map(lambda s: s.split(':'), value.strip(',').split(','))))
            self.evicts_unknown -= sum(results['evictedby'].values())
        self.sites[siteid] = AllocationSite(stack, **results)
      else:
        raise ValueError('Invalid format %s' % line)
    #print ', '.join([ '%s:%d' % (k, v) for k, v in hitwhere_global.items() if v ])
    #print ', '.join([ '%s:%d' % (k, v) for k, v in hitwhere_unknown.items() if v ])
    #print evicts_global, evicts_unknown

  def write(self, obj):
    sites_sorted = sorted(self.sites.items(), key = lambda (k, v): v.totalloads + v.totalstores, reverse = True)
    site_names = dict([ (siteid, '#%d' % (idx+1)) for idx, (siteid, site) in enumerate(sites_sorted) ])
    for siteid, site in sites_sorted:
      print >> obj, 'Site %s:' % site_names[siteid]
      print >> obj, '\tCall stack:'
      for eip in site.stack:
        print >> obj, '\t\t%s' % self.functions[eip]
      print >> obj, '\tAllocations: %d' % site.numallocations
      print >> obj, '\tTotal allocated: %d' % site.totalallocated
      print >> obj, '\tHit-where:'
      print >> obj, '\t\t%-15s: %12d          ' % ('Loads', site.totalloads),
      print >> obj, '\t%-15s: %12d' % ('Stores', site.totalstores)
      for hitwhere in self.hitwheres:
        if site.hitwhereload.get(hitwhere) or site.hitwherestore.get(hitwhere):
          cnt = site.hitwhereload[hitwhere]
          print >> obj, '\t\t  %-15s: %s' % (hitwhere, format_abs_ratio(cnt, site.totalloads)),
          cnt = site.hitwherestore[hitwhere]
          print >> obj, '\t  %-15s: %s' % (hitwhere, format_abs_ratio(cnt, site.totalstores))
      print >> obj, '\tEvicted-by:'
      evicts = sorted(filter(lambda (siteid, cnt): cnt > 10, site.evictedby.items()), key = lambda (siteid, cnt): cnt, reverse = True)
      for siteid, cnt in evicts[:10]:
        print >> obj, '\t\t%-15s: %12d' % (site_names.get(siteid, 'other'), cnt)
      print >> obj

    print >> obj, 'By hit-where:'
    totalloads = sum(self.hitwhere_load_global.values())
    totalstores = sum(self.hitwhere_store_global.values())
    for hitwhere in self.hitwheres:
      if self.hitwhere_load_global[hitwhere] + self.hitwhere_store_global[hitwhere]:
        totalloadhere = self.hitwhere_load_global[hitwhere]
        totalstorehere = self.hitwhere_store_global[hitwhere]
        print >> obj, '\t%s:' % hitwhere
        print >> obj, '\t\t%-15s: %s' % ('Loads', format_abs_ratio(totalloadhere, totalloads)),
        print >> obj, '\t%-15s: %s' % ('Stores', format_abs_ratio(totalstorehere, totalstores))
        for siteid, site in sorted(self.sites.items(), key = lambda (k, v): v.hitwhereload.get(hitwhere, 0) + v.hitwherestore.get(hitwhere, 0), reverse = True):
          if site.hitwhereload.get(hitwhere) > .001 * totalloadhere or site.hitwherestore.get(hitwhere) > .001 * totalstorehere:
            print >> obj, '\t\t  %-15s: %s' % (site_names[siteid], format_abs_ratio(site.hitwhereload.get(hitwhere), totalloadhere)),
            print >> obj, '\t  %-15s: %s' % (site_names[siteid], format_abs_ratio(site.hitwherestore.get(hitwhere), totalstorehere))
        if self.hitwhere_load_unknown.get(hitwhere) > .001 * totalloadhere or self.hitwhere_store_unknown.get(hitwhere) > .001 * totalstorehere:
          print >> obj, '\t\t  %-15s: %s' % ('other', format_abs_ratio(self.hitwhere_load_unknown.get(hitwhere), totalloadhere)),
          print >> obj, '\t  %-15s: %s' % ('other', format_abs_ratio(self.hitwhere_store_unknown.get(hitwhere), totalstorehere))

if __name__ == '__main__':

  import getopt

  def usage():
    print '%s  [-d <resultsdir (.)> | -o <outputdir>]' % sys.argv[0]
    sys.exit(1)

  HOME = os.path.dirname(__file__)
  resultsdir = '.'
  outputdir = None

  try:
    opts, cmdline = getopt.getopt(sys.argv[1:], "hd:o:")
  except getopt.GetoptError, e:
    # print help information and exit:
    print >> sys.stderr, e
    usage()
  for o, a in opts:
    if o == '-h':
      usage()
      sys.exit()
    if o == '-d':
      resultsdir = a
    if o == '-o':
      outputdir = a

  result = MemoryTracker(resultsdir)
  result.write(file(os.path.join(outputdir, 'sim.memoryprofile'), 'w') if outputdir else sys.stdout)
