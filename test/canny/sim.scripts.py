import sys
sys.argv = [ "/workspace/sniper/sniper-6.1/scripts/periodic-stats.py", "1000:2000" ]
execfile("/workspace/sniper/sniper-6.1/scripts/periodic-stats.py")
sys.argv = [ "/workspace/sniper/sniper-6.1/scripts/markers.py", "markers" ]
execfile("/workspace/sniper/sniper-6.1/scripts/markers.py")
