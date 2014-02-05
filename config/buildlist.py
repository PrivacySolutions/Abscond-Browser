# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''A generic script to add entries to a file 
if the entry does not already exist.

Usage: buildlist.py <filename> <entry> [<entry> ...]
'''
from __future__ import print_function

import sys
import os
from utils import lockFile

def addEntriesToListFile(listFile, entries):
  """Given a file |listFile| containing one entry per line,
  add each entry in |entries| to the file, unless it is already
  present."""
  lock = lockFile(listFile + ".lck")
  try:
    if os.path.exists(listFile):
      f = open(listFile)
      existing = set(x.strip() for x in f.readlines())
      f.close()
    else:
      existing = set()
    for e in entries:
      if e not in existing:
        existing.add(e)
    f = open(listFile, 'w')
    f.write("\n".join(sorted(existing))+"\n")
    f.close()
  finally:
    lock = None

if __name__ == '__main__':
  if len(sys.argv) < 3:
    print("Usage: buildlist.py <list file> <entry> [<entry> ...]",
          file=sys.stderr)
    sys.exit(1)
  addEntriesToListFile(sys.argv[1], sys.argv[2:])
