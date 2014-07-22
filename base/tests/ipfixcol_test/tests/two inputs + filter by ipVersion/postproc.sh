rm -f output

for out in out.ipfix.*; do
	ipfixviewer $out >> output
done

rm -f out.ipfix.*

