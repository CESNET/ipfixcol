$IPFIX_TEST_IPFIXCOL -v -1 -c listen.xml -i $IPFIX_TEST_INTERNALCFG -e $IPFIX_TEST_ELEMENTCFG &
echo $! > pid
sleep 1
