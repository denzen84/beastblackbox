#!/bin/bash

foldname=`date +%s.%N`

nc 192.168.1.129 30005 > ulss7-beast-bin-utc--$foldname--.log

#exit 0
