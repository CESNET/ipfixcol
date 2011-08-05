BEGIN {
    print "const char *protocols[] = {"; 
    currentVal = 0;
    maxNum = 255;
}


!/^#/ {
    while(currentVal != $2) {
        print "\""currentVal"\",";
        currentVal++;
    }
    print "\""$3"\",";
    currentVal++;
}

END {
    while (currentVal <= maxNum) {
        print "\""currentVal"\",";
        currentVal++;
    }
    print "};";
}
