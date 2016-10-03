##<a name="top"></a>ProfileStats intermediate plugin
###Plugin description

The plugin creates and updates RRD statistics per profile and channel.
Based on a configuration of profiles maintained by **profiler** plugin,
corresponding databases are updated every interval. If RRD databases already
exist, old ones will be used for updates and a previous content will be
preserved. The databases store high quality i.e. short term records (defined
by a size of update interval) for last 3 months and low quality i.e. long term
records (one record per day) for 5 years. Each RRD consists of following Data
Sources:

| Name          | Description                                                |
|---------------|------------------------------------------------------------|
| flows         | Number of all flows per second                             |
| flows_tcp     | Number of TCP flows per second                             |
| flows_udp     | Number of UDP flows per second                             |
| flows_icmp    | Number of ICMP flows per second                            |
| flows_other   | Number of other flows (i.e. not TCP/UDP/ICMP) per second   |
| packets       | Number of all packets per second                           |
| packets_tcp   | Number of TCP packets per second                           |
| packets_udp   | Number of UDP packets per second                           |
| packets_icmp  | Number of ICMP packets per second                          |
| packets_other | Number of other packets (i.e. not TCP/UDP/ICMP) per second |
| packets_max   | Maximum packets from one flow per interval                 |
| packets_avg   | Average number of packets per flow and per interval        |
| traffic       | Number of all bytes per second                             |
| traffic_tcp   | Number of TCP bytes per second                             |
| traffic_udp   | Number of UDP bytes per second                             |
| traffic_icmp  | Number of ICMP bytes per second                            |
| traffic_other | Number of other bytes (i.e. not TCP/UDP/ICMP) per second   |
| traffic_max   | Maximum bytes from one flow per interval                   |
| traffic_avg   | Average number of bytes per flow and per interval          |

Each source have consolidation functions "MAX" and "AVERAGE". Consolidation
functions consilidate primary data points (in this case, a record per
interval) via an agregation functions. This is necessary, for example,
for storing long term statistics, because one long term RRD recond is
generated from multiple data points (even several thousands). "AVERAGE" is
an average of the data points and "MAX" is the larges of the data points.
For short term statistics (for example displaying statistics since last hour)
consolidation functions usuallly generate same results.

An RRD database for a profile is created in "/rrd/" subdirectory of
a main directory of the profile. For each channel of the profile is
created a separated RRD database in "/rrd/channels/" subdirectory.
For example, for profile "live" and its channels "ch1" and "ch2", the plugin
will create databases: &lt;profile_dir&gt;/rrd/live.rrd,
&lt;profile_dir&gt;/rrd/channels/ch1.rrd and
&lt;profile_dir&gt;/rrd/channels/ch2.rrd.

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<intermediatePlugin>
	<name>profilestats</name>
	<file>/usr/share/ipfixcol/plugins/ipfixcol-profilestats-inter.so</file>
	<threadName>profilestats</threadName>
</intermediatePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
	 intermediate           profilestats            profilestats          /usr/share/ipfixcol/plugins/ipfixcol-profilestats-inter.so
```

The collector must be configured to use ProfileStats plugin in startup.xml
configuration. The **profiler** plugin _must_ be placed before ProfileStats
in the IPFIXcol pipeline. Otherwise no profiles description will be
available and no databases will be created/updated.

Example **startup.xml** configuration:

```xml
<profilestats>
        <interval>300</interval>
</profilestats>
```
*  **interval** Update interval (in seconds). Size of the interval
significantly infuence size of databases. [min: 5, max: 3600, default: 300]

###How to generate a graph (with RRD tools)
For example, let us consider a profile with two channels, "ch1" and "ch2".
The channel "ch1" accepts all records with IPv4 addresses and channel
"ch2" accepts all records with IPv6 address. We want to create a graph of
a number of TCP flows per second since last 2 hours (i.e. -7200 seconds).

First, we have to fetch data from both RRD files (one file per channel).
This function fetch data from an RRD file:

```
DEF:<vname>=<file>:<source>:<cf>
```

("vname" is variable name of a time series, "file" is path
to an RRD file, "source" is a name of a Data Source in the RRD file
and "cf" is a Consolidation function).
To create the time series called "IP4" with the number of TCP
flows (Data Series "flows_tcp") from the file
"ch1.rrd" use command "DEF:IP4=ch1.rrd:flows_tcp:AVERAGE". Similarly
for the IP6 time series.

Finally, any time series can displayed, for example, as area using the command:

```
AREA:<vname>:<#color>:<legend>
```

("vname" is variable name of a time series, "color" is an RGB(A) color,
"legend" is a legend of the series).

Example rrdtool command:

```
    rrdtool graph traffic.png              \
      --full-size-mode -w 640 -h 480       \
      --start -7200 --end now              \
      --title "TCP flows"                  \
      --lower-limit 0 --alt-y-grid         \
      --vertical-label "flows per second"  \
      DEF:IP4=ch1.rrd:flows_tcp:AVERAGE    \
      DEF:IP6=ch2.rrd:flows_tcp:AVERAGE    \
      AREA:IP4#66CC00:"IPv4"               \
      AREA:IP6#FF0000:"IPv6":STACK
```

Tip: Say you want to display a traffic speed in bits per seconds (instead
of bytes per seconds as stored in the database). You have to define
a calculation on a variable.
```
CDEF:<vname>=<expression>
```
For example:
```
      ...
      DEF:bytes=ch1.rrd:traffic:AVERAGE    \
      CDEF:bits=bytes,8,*                  \
      AREA:bits#FF0000:"bits per second"   \
      ...
```

###Note
It is highly recommended to use only one instance of the plugin
in the configuration of IPFIXcol, because external RRD library is not
very thread-safety.

[Back to Top](#top)
