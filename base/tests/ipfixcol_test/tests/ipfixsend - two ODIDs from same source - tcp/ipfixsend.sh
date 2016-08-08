$IPFIX_TEST_IPFIXSEND -i ../ipfix_data/04-odid256,512.ipfix -d 127.0.0.1 -p 4739 -t tcp -n 1 -S 25

wait $!
