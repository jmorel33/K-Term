#!/bin/bash
gcc -o verify_diagnostics tests/verify_diagnostics.c -I. -lpthread -lm
if [ $? -eq 0 ]; then
    ./verify_diagnostics
else
    echo "Compilation failed"
fi
