#/bin/bash

cd a4/src
./configure

cd kern/conf
./config ASST4

cd ../compile/ASST4
bmake depend
bmake

bmake install

cd ../../..
bmake
bmake install

