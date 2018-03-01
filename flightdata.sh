#!/bin/bash

foldname=`date +%s-%N`

nc 192.168.1.129 30005 > $foldname-ulss7-beast-bin.log

#exit 0
