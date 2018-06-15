#!/bin/bash
# gdbserver :9091 
./canny \
    -in ../shared/lena.pgm \
    -out outputs/lena.pgm \
    -sigma 0.33 \
    -tlow 0.45098039215686275 \
    -thigh 0.8941176470588236 \
    -num-frames 1 -save-output \
    -read-ber 1E-1 \
    -write-ber 1E-1 \
    -report-error \
    -sampling-freq 1 \
    -repeat-frame 1 \
    -control-mode 0