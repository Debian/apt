#!/bin/sh
echo "Compiling the tests ..."
make
echo "Running all testcases ..."
PATH=$(pwd)/../../build/bin
for testapp in $(/bin/ls ${PATH}/*_libapt_test)
do
	echo -n "Testing with \033[1;35m$(/usr/bin/basename ${testapp})\033[0m ... "
	LD_LIBRARY_PATH=${PATH} ${testapp} && echo "\033[1;32mOKAY\033[0m" || echo "\033[1;31mFAILED\033[0m"
done
