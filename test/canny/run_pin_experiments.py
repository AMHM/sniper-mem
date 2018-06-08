#!/usr/bin/python
##!/data/apps/python/2.7.2/bin/python

import sys
import subprocess
import datetime
from collections import defaultdict
import os

MAX_FRAMES = 100
FPS = 30
REPEAT_FRAMES = 1

TL = 0.45098039215686275
TH = 0.8941176470588236

HPC = False
HOME = os.environ["HOME"]
PIN_BASE = HOME + '/phd/pin/pin-3.2-81205-gcc-linux/'

CMD = '''{0:}/pin \
-t {0:}/source/tools/ManualExamples/obj-intel64/memapprox.so -- \
   {0:}/examples/canny/bin/x86/canny \
-in {0:}/examples/inputs/{1:}/{1:} \
-out {0:}/examples/canny/outputs/{1:}/{1:} \
-sigma 0.33 \
-tlow {2:} \
-thigh {3:} \
-num-frames {4:} \
-report-error \
-read-ber {5:} \
-write-ber {6:} \
-sampling-freq 1000 \
-repeat-frame {7:}'''

BERs = [
        (0,       1E-6), 
        (0,  1*25*1E-6), 
        (0,  2*25*1E-6), 
        (0,  3*25*1E-6), 
        (0,  4*25*1E-6), 
        (0,  5*25*1E-6), 
        (0,  6*25*1E-6), 
        (0,  7*25*1E-6), 
        (0,  8*25*1E-6), 
        (0,  9*25*1E-6), 
        (0, 10*25*1E-6), 
        (0, 11*25*1E-6), 
        (0, 12*25*1E-6), 
        (0, 13*25*1E-6), 
        (0, 14*25*1E-6), 
        (0, 15*25*1E-6), 
        (0, 16*25*1E-6), 
        (0, 17*25*1E-6), 
        (0, 18*25*1E-6), 
        (0, 19*25*1E-6), 
        (0, 20*25*1E-6), 
        (0, 21*25*1E-6), 
        (0, 22*25*1E-6), 
        (0, 23*25*1E-6), 
        (0, 24*25*1E-6), 
        (0, 25*25*1E-6), 
        (0, 26*25*1E-6), 
        (0, 27*25*1E-6), 
        (0, 28*25*1E-6), 
        (0, 29*25*1E-6), 
        (0, 30*25*1E-6), 
        (0, 31*25*1E-6), 
        (0, 32*25*1E-6), 
        (0, 33*25*1E-6), 
        (0, 34*25*1E-6), 
        (0, 35*25*1E-6), 
        (0, 36*25*1E-6), 
        (0, 37*25*1E-6), 
        (0, 38*25*1E-6), 
        (0, 39*25*1E-6), 
        (0, 40*25*1E-6),        
        ]

VIDEOS = {
    # 'bunny': 1000,
    'chromecast': 1000,
    # 'marymax': 1000,
    # 'bridge_close': 2001,
    # 'city': 600,
    # 'container': 300,
    # 'crew': 600,
    # 'football': 360, 
    # 'galleon': 360,
    # 'ice': 480,
    # 'news': 300,
    # 'soccer': 600,
    # 'students': 1007,
    # 'tempete': 260,
}

def create_frames(scene, fps):
  for ext in ['_cif.y4m', '.mp4']:
    cmd = 'ffmpeg -i ../inputs/{:}/{:}{:} -r {:}/1 ../inputs/{:}/{:}_%04d.pgm'.format(scene, scene, ext, fps, scene, scene)
    subprocess.call(cmd, shell=True)

def delete_frames(scene):
  cmd = 'rm -rf ../inputs/{:}/*.pgm'.format(scene)
  subprocess.call(cmd, shell=True)

def run_pin_experiments(just_create_frames = False, remove_frames = False):
  timestamp = datetime.datetime.now().strftime('%Y%m%dT%H%M%S')
  for scene in VIDEOS:

    create_frames(scene, FPS)

    if just_create_frames:
      continue

    cmd = 'mkdir -p outputs/{:}'.format(scene)
    subprocess.call(cmd, shell=True)

    cmd = 'rm -r outputs/{:}/*'.format(scene)
    subprocess.call(cmd, shell=True)

    cmd = 'cp run_pin_experiments.py outputs/{:}/'.format(scene)
    subprocess.call(cmd, shell=True)

    for ber in BERs:

      cmd = CMD.format(PIN_BASE, scene, TL, TH, min(MAX_FRAMES,(VIDEOS[scene]/30)*FPS), ber[0], ber[1], REPEAT_FRAMES)
      print (cmd)
      simout = '{:}/examples/canny/outputs/{:}/{:}_{:}_{:}.log'.format(PIN_BASE, scene, timestamp, ber[0], ber[1])

      if HPC:
        simout_f = open('run.sh',"w")
        simout_f.write('#!/bin/bash \n')
        simout_f.write('#$ -N beep \n')
        simout_f.write('#$ -q drg,pub64 \n')
        simout_f.write('#$ -pe openmp 1 \n\n')
        simout_f.write(cmd + ' > ' + simout)
        subprocess.call('qsub run.sh', shell=True)
      else:
        simout_f = open(simout,"w")
        subprocess.call(cmd, stdout=simout_f, stderr=simout_f, shell=True)
  if remove_frames:
    delete_frames(scene)

if __name__ == "__main__":
  run_pin_experiments(just_create_frames = False, remove_frames = False)