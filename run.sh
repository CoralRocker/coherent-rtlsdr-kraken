#!/bin/bash

CUSTOM_RTLSDR_LOC=./rtl-sdr/src/librtlsdr.so.0


make

sudo LD_PRELOAD=$CUSTOM_RTLSDR_LOC ./coherentrtlsdr -C kraken.cfg -b 262144 -f 1626000000
