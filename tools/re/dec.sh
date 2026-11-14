#!/bin/bash
# usage: dec.sh out.c addr...   (decompile at addresses, creating functions if needed)
out=$1; shift
/opt/ghidra/ghidra_11.2.1_PUBLIC/support/analyzeHeadless /tmp/claude-0/gproj2 hh2d -process HH2D.exe -noanalysis -scriptPath /tmp/claude-0/gscripts -postScript DecompileAt.java "$out" "$@" > /tmp/claude-0/ghidra3.log 2>&1
cat "$out"
