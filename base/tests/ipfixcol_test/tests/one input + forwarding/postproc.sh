kill $(cat pid)
rm -f pid

diff out-first* out-second* > output
# make the test fail when diff has trouble (e.g. missing file)
[ $? -eq 2 ] && echo "fail" > output

rm out-*