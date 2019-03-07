#!/usr/bin/env python3

import os,sys
import shutil

def cpfile(s, d):
  if not os.path.exists(d):
    os.mkdir(d)

  s = s.rstrip(os.sep)
  d = d.rstrip(os.sep)
  basename = os.path.basename(s)

  if os.path.isfile(s):
    print("cp %s ==> %s" % (s, d))
    shutil.copy(s, d + os.sep + basename )
  else:
    alllist = os.listdir(s)
    for ss in alllist:
      cpfile(s + os.sep + ss, d + os.sep + basename )

def m_cp(*args):
  """copy files/folders to dest folder"""
  if (not args or len(args) < 2) :
    print("parameter is invalid")
    return

  src = args[0:len(args)-1]
  dst = args[len(args)-1]

  if not os.path.exists(dst) :
    os.mkdir(dst)

  for s in src:
    print("cp %s ==> %s" % (s, dst))
    cpfile(s, dst)


if __name__ == "__main__":
  m_cp(*sys.argv[1:])
