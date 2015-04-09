$IPFIX_TEST_IPFIXSEND -i ../ipfix_data/01-odid0.ipfix -d 127.0.0.1 -p 4739 -t udp -n 1 -S 25 &
$IPFIX_TEST_IPFIXSEND -i ../ipfix_data/01-odid4.ipfix -d 127.0.0.1 -p 4739 -t udp -n 1 -S 25

wait $!
