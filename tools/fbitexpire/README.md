##fbitexpire

###Tool description

This tool is used to control free disk space and remove old FastBit data if needed. It consist of several parts.

###Watcher

Watcher thread uses inotify (inotify-cxx, it is included within tool sources) events to register newly created folders and informs scanner about their existence.

###Scanner

Scanner keeps directory tree with informations about their size and age (time last modified). If disk usage reaches given limit it removes the oldest folder(s) from tree and tells cleaner to remove them from disk.

###Cleaner

Simple part which waits on request from scanner and removes folders.

###PipeListener

You can communicate with fbitexpire on it's pipe. PipeListener parses these messages and informs other parts about changes etc. 

These requests can be send into pipe:

*  to stop fbitexpire
*  to rescan some directory
*  to change maximal size limit on the fly
*  to change watermark limit (the lower limit when removing old folders) on the fly

###Example

```sh
fbitexpire -d 4 -s 5000 /data/collector/
```
 Watch subdirectories at /data/collector. Depth is set to 4 so we watch directories of maximal depth /data/collector/1/2/3/4. Watched size is 5GB.
 
```sh
fbitexpire -c -s 53G -w 250M -p /tmp/expirepipe
````
Change settings of fbitexpire listening on pipe /tmp/expirepipe. Change size to 53 GB and watermark limit to 250 MB.

[Back to Top](#top)
