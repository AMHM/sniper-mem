#!/bin/bash

##############################################
PIN_PATH=/home/majid/phd/pin/pin-3.2-81205-gcc-linux
PIN_BIN=$PIN_PATH/pin 
MEM_APPROX_TOOL=$PIN_PATH/source/tools/ManualExamples/obj-intel64/memapprox.so

##############################################
APP_BIN=$PIN_PATH/examples/canny/bin/x86/canny

VIDEO=mix4
FPS=30

INPUT=$PIN_PATH/examples/inputs/$VIDEO/$VIDEO
OUTPUT=$PIN_PATH/examples/canny/outputs/$VIDEO/$VIDEO

# default
TL=0.45

if [[ "$VIDEO" = "mix1" ]]
then
	TL=0.1
fi
if [[ "$VIDEO" = "mix2" ]]
then
	TL=0.25
fi
if [[ "$VIDEO" = "mix3" ]]
then
	TL=0.33
fi
if [[ "$VIDEO" = "mix4" ]]
then
	TL=0.45
fi

TH=`echo "2*$TL" | bc -l`

NUM_FRAMES=1200
REPEAT_FRAME=1
SAMPLING_FREQ=20

INITIAL_READ_BER=0
INITIAL_WRITE_BER=0

CALIBRATE=-calibrate
REPORT_ERROR=-report-error
# SAVE_OUTPUT=-save-output

##############################################
ffmpeg -i ../inputs/$VIDEO/"${VIDEO}_cif.y4m" -r $FPS/1 -vframes $NUM_FRAMES ../inputs/$VIDEO/"${VIDEO}_%04d.pgm"
ffmpeg -i ../inputs/$VIDEO/"${VIDEO}.mp4" -r $FPS/1 -vframes $NUM_FRAMES ../inputs/$VIDEO/"${VIDEO}_%04d.pgm"

echo "==============================================================================="
echo $VIDEO $NUM_FRAMES $SAMPLING_FREQ
echo "==============================================================================="

##############################################
$PIN_BIN \
-t $MEM_APPROX_TOOL -- \
$APP_BIN \
-in $INPUT \
-out $OUTPUT \
-sigma 0.33 \
-tlow $TL \
-thigh $TH \
-num-frames $NUM_FRAMES \
$SAVE_OUTPUT \
$CALIBRATE \
-read-ber $INITIAL_READ_BER \
-write-ber $INITIAL_WRITE_BER \
$REPORT_ERROR \
-sampling-freq $SAMPLING_FREQ \
-repeat-frame $REPEAT_FRAME \
-control-mode pid