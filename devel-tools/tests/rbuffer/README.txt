This tool uses the ipfixcol ring buffer queue with configurable number of threads.

Each thread have specified delay, so that different speeds can be simulated.

Threads try to detect invalid meomry read by checkind that ODID entry is set properly,
however this still might mean, that the threads are reading recently freed memory,
so for better precision it might be good idead to modify the queue to set the ODID to
something else on free.

For detailed information see the code.
