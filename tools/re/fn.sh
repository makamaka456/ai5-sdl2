#!/bin/bash
# print decompiled function(s) by address
for a in "$@"; do a=$(printf "%08x" 0x$a); awk -v F="FUN_$a" '$0 ~ "^// ===== "F" " {p=1; print; next} p && /^\/\/ ===== / {exit} p' /tmp/claude-0/hh2d.c; done
