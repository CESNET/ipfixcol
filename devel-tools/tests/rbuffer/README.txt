This tool uses the ipfixcol ring buffer queue with configurable number of threads.

Each thread have specified delay, so that different speeds can be simulated.

Threads try to detect invalid meomry read by checkind that ODID entry is set properly.
The ODID is expected to increase in each message, if it does not, error is
reported.

For detailed information see the code.
