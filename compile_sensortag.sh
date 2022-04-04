#!/bin/bash
if [ "$#" -eq 0 ]; then
    echo "No binary file provided"
    exit 1
elif [ "$#" -ne 2 ]; then
    echo "You must enter 2 binary files"
    exit 1
fi

make TARGET=srf06-cc26xx BOARD=sensortag/cc2650 $1 $2 CPU_FAMILY=cc26xx
