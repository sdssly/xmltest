#!/bin/bash

if [ -z $1 ] ; then
	exit 0
fi
echo "$1" >> /tmp/dbg

if [ -f $1 ]; then
	dir=`dirname $1`
	
	mkdir -p "/tmp/$dir"
	cp -a $1 /tmp/$dir/
fi 

if [ -L $1 ]; then
	dir=`dirname $1`
	mkdir -p "/tmp/$dir"
	cp -a $1 /tmp/$dir/
	realfile=`readlink -f $1`
	cp -a $realfile /tmp//$dir/
 echo "link ..." >> /tmp/dbg
fi 


#lines=`ldd $1`

#echo $lines

#for line in $lines; do
#echo "line=[$line]"

#done 
