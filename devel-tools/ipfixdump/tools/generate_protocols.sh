#!/bin/bash
if [ -f /etc/protocols ] 
then
    awk -f parse_protocols.awk /etc/protocols > ../src/protocols.cpp
    echo file protocols.cpp created successfully
else
    echo file /etc/protocols does not exist!
fi