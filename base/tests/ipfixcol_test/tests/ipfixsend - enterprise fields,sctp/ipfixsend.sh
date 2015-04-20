$IPFIX_TEST_IPFIXSEND -i ../ipfix_data/enterprise-from-nfv9.ipfix -d 127.0.0.1 -p 4739 -t sctp -n 1

wait $!