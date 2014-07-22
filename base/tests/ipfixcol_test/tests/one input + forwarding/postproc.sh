kill $(cat pid)
rm -f pid

diff out-first* out-second* > output

rm out-*