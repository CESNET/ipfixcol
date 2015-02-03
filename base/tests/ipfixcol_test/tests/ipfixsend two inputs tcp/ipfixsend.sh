$IPFIX_TEST_IPFIXSEND -i ../ipfix_data/01-odid0.ipfix -d 127.0.0.1 -p 4739 -t tcp -n 1 &
$IPFIX_TEST_IPFIXSEND -i ../ipfix_data/01-odid4.ipfix -d 127.0.0.1 -p 4739 -t tcp -n 1

wait $!