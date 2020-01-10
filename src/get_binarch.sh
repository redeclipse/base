#!/bin/sh
# given a target triple, prints the binary architecture name

case $1 in
    x86_64*)    echo amd64;;
    i[4-6]86*)  echo x86;;
    armv*)      echo arm;;
    aarch64*)   echo aarch64;;
    powerpc64*) echo ppc64;;
    powerpc*)   echo ppc;;
    *) ;; # unknown
esac

exit 0
