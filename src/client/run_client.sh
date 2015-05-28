#!/bin/bash
LOGINNAME=linuxmeijin1
THREADS=3
HASHSIZE=300
while true
do
	./one_godwhale --threads=${THREADS} --hash=${HASHSIZE} 52.68.32.1 4083 ${LOGINNAME}
done

