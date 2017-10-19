# <a name="top"></a>Profiler intermediate plugin

The ipfix-profiler-inter plugin is an intermediate plugin for IPFIXcol (IPFIX collector).
It profiles IPFIX data records and fills in metadata information according to given set of
profiles and their channels.

## Introduction to profiling


The goal of flow profiling is multi-label classification (based on a set of rules) into
user-defined groups. These labels can be used for further flow processing. The basic terminology
includes profiles and channels.

A profile is a view that represents a subset of data records received by a collector. Consequently,
this allows surfacing only the records that a user needs to see. Each profile contains one or
more channels, where each channel is represented by a filter and sources of flow records.
If a flow satisfies a condition of any channel of a profile, then the flow will be labeled with
the profile and the channel. Any flow can have as many labels as possible. In other words, it can
belong to multiple channels/profiles at the same time.

For example, let us consider that you store all flows and besides you want to store only flows
related to email communications (POP3, IMAP, SMTP). To do this, we can create a profile "emails"
with channels "pop3", "imap" and "smtp". When a flow with POP3 communication (port 110 or 995)
is classified, it will meet the condition of the "pop3" channel and will be labeled as the flow that
belongs to the profile "emails" and the channel "pop3".

Example of a profile hierarchy:
```
live (channels: ch1, ch2)
├── emails (channels: pop3, smtp, imap)
└── buildings (channels: net1, net2, net3)
    ├── office (channels: http, voip)
    └── factory (channels: http, voip)
```

The profiles can be nested and create a tree hierarchy (as shown above). A flow source of
a channel in a profile can be only one or more channels of the direct parent of the profile
to which the channel belongs. For example, the channel "http" in the profile "office" can use
only "net1", "net2" or "net3" channels as sources. The exception is the highest level i.e. "live"
profile. This profile must be always present, has exactly this name, and its channels will receive
all flow records intended for profiling.

How does flow profiling work? In a nutshell, if a record satisfies a filter of a channel,
it will be labeled with the channel and the profile to which the channel belongs and will
be also sent for evaluation to all subscribers of the channel.
For example, let us consider the tree hierarchy above. All flow records will be always sent
to all channels of "live" profile as mentioned earlier. If a flow record satisfies the
filter of the channel "ch1", the record will be labeled with the profile "live" and the channel
"ch1". Because the flow belongs to the channel "ch1" it will be also sent for evaluation to all
subscribers of this channel i.e. to the channels of the profiles "emails" and "buildings" that
have this channel ("ch1") in their source list.
If the record doesn't satisfy the filter, it will not be distributed to the subscribers.
However, if the record satisfies the channel "ch2" and the channels of the profiles "emails" and
"buildings" are also subscribers of the channel "ch2", the record will be sent them too.
Thus, the record can get to any channel in different ways but labeled can be only once.

For now, following plugins support profiling provided by this plugin:
- [lnfstore](https://github.com/CESNET/ipfixcol/tree/master/plugins/storage/lnfstore) - Convert and store IPFIX records into NfDump files. For more information see manual page
   of the plugin.
- [profilestats](https://github.com/CESNET/ipfixcol/tree/master/plugins/intermediate/profile_stats) - Create and update RRD statistics per profile and channel. For more information
   see manual page of the plugin.

## Plugin configuration

 The collector must be configured to use Profiler plugin in the startup.xml configuration. The profiler
 plugin must be placed into the intermediate section before any other plugins that use profiling
 results. Otherwise, no profiling information will be available for these plugins.

```xml
<collectingProcess>
    ...
    <profiles>/path/to/profiles.xml</profiles>
</collectingProcess>
...
<intermediatePlugins>
    ...
    <profiler>
    </profiler>
    ...
</intermediatePlugins>
```

The plugin does not accept any parameters, but the Collecting process must define parameter
`<profiles>` with an absolute path to the profiling configuration.

## Structure of the profiling configuration


This section describes parts of the XML configuration file. Example configuration for an email
subprofile can be found in a section below.

Keep in mind that profile and channel names must match a format that corresponds to the variables
in the C language. In other words, a name can have letters (both uppercase and lowercase), digits
and underscore only. The first letter of a name should be either a letter or an underscore.
Consequently, the names must match regular expression `[a-zA-Z_][a-zA-Z0-9_]*`.

Warning: Using complicated filters and multiple nested profiles have a significant impact on
the throughput of the collector!

### Profile (\<profile\>)
```xml
<profile name="...">
    <type>...</type>
    <directory>...</directory>
    <channelList>...</channelList>
    <subprofileList>...</subprofileList>
</profile>
```
Each profile has a name attribute (i.e. `<name>`) for identification among other profiles.
The attribute must be unique only among a group of profiles those belong to the common
parent profile. In the definition of each profile must be exactly one definition of following
elements:

- `<type>` \
    Profile type ("normal" or "shadow"). Normal profile means that IPFIXcol plugins should
    store all valuable data (usually flow records and metadata). On the other hand,
    for shadow profiles, the plugins should store only metadata. For example: In case of
    the lnfstore plugin, only flows of normal profiles are stored. Others are ignored.
- `<directory>` \
    The absolute path to a directory. All data records and metadata that belong to the profile
    and its channels will be stored here. The directory MUST be unique for each profile!
- `<channelList>` \
    List of one or more channels (see the section below about channels)

Optionally, each profile can contain up to one definition of an element:
- `<subprofileList>` \
  List of subprofiles that belongs to the profile. The list can be empty.

### Channel \<channel\>
```xml
<channel name="...">
    <sourceList>
        ...
        <source>...</source>
        ...
    </sourceList>
    <filter>...</filter>
</channel>
```

Each channel has a name attribute (i.e. `<name>`) for unique identification amongst other
channels within a profile. Each channel must have exactly one definition of:
- `<sourceList>` \
  List of flow sources. In this case, a source of records should not be confused with
  an IPFIX/NetFlow exporter. The source is basically a channel from a parent profile
  from which this channel will receive flow records. Each source must be specified in
  an element `<source>`. \
  If the channel receives data from all parent channels, the list of channels can be replaced
  with only one source: `<source>*</source>`. Channels in the "live"
  profile always have to use this notation.

Each channel within any "shadow" profile must receive from all channels from a parent profile
i.e. it must always use only '*' source! It is due to the fact that for later evaluation of
queries over shadow profiles (for example by fdistdump or other tools) the information about
parent channels that belong to every flow has been already lost.

Optionally, each channel may contain one:
- `<filter>` \
  A flow filter expression that will be applied to flow records received from sources.
  The records that satisfy the specified filter expression will be labeled with this
  channel and the profile to which this channel belongs.
  If the filter is not defined, the expression is always evaluated as true.

Warning: User must always make sure that intersection of records that belong to multiple
channels of the same profile is always empty! Otherwise, the record can be stored multiple
times (in case of lnfstore) or added to the summary statistic of the profile multiple times
(in case of profilestats).

## Example configuration

Following configuration is based on the hierarchy mentioned earlier but few parts have been
simplified.

```xml
    <profile name="live">
        <type>normal</type>
        <directory>/some/directory/live/</directory>
        <channelList>
            <channel name="ch1">
                <sourceList><source>*</source></sourceList>
                <filter>odid 10</filter>
            </channel>
            <channel name="ch2">
                <sourceList><source>*</source></sourceList>
                <filter>odid 20</filter>
            </channel>
        </channelList>

        <subprofileList>
            <profile name="emails">
                <type>normal</type>
                <directory>/some/directory/emails/</directory>

                <channelList>
                    <channel name="pop3">
                        <sourceList>
                            <source>ch1</source>
                            <source>ch2</source>
                            <!-- or just <source>*</source> -->
                        </sourceList>
                        <filter>port in [110, 995]</filter>
                    </channel>

                    <!--
                    <channel name="smtp">
                        ...
                    </channel>
                    -->
                </channelList>
            </profile>

            <!--
            <profile name="buildings">
                ...
                <subprofileList>
                    <profile name="office">
                        ...
                    </profile>
                    <profile name="factory">
                        ...
                    </profile>
                </subprofileList>
            </profile>
            -->
        </subprofileList>
    </profile>
```

### Tips

If you need to distinguish individual flow exporters, we highly recommend configuring each exporter
to use unique Observation Domain ID (ODID) (IPFIX only), configure each channel of the "live"
profile to represent one exporter and use filter keyword "odid" (see the filter syntax for more
details and limitations). If the ODID method is not applicable in your situation, you can also
use "exporterip", "exporterport", etc. keywords, but be aware, this doesn't
make sense in case of the distributed collector architecture because all flows are sent to the one
active proxy collector and then redistributed by the proxy to subcollectors that perform profiling.
From the point of view of any subcollector the proxy is the exporter, therefore these ODID
replacements don't work as expected. On the other hand, the ODID always works.

If you want to make sure that your configuration file is ready to use, you can use a tool called
"ipfixcol-profiles-check". Use "-h" to show all available parameters.

## Filter syntax

The filter syntax is based on the well-known nfdump tool. Although keywords must be written with
lowercase letters. Any filter consists of one or more expressions `expr`.
Any number of `expr` can be linked together: `expr and expr, expr or expr, not expr and (expr).`

An expression primitive usually consists of a keyword (a name of an Information Element),
optional comparator, and a value. By default, if the comparator is omitted, equality
operator `=` will be used. Numeric values can use scaling of following supported scaling
factor: k, m, g. The factor is 1000.

Following comparators `comp` are supported:
- equals sign (`=`, `==` or `eq`)
- less than (`<` or `lt`)
- more than (`>` or `gt`)
- like/binary and (`&`);

Below is the list of the most frequently used filter primitives that are universally supported.
If you cannot find the primitive you are looking for, try to use the corresponding *nfdump* expression
or just use the name of IPFIX Information Element. If you need to preserve compatibility with
*fdistdump*, you have to use only nfdump expressions!

- _IP version_ \
  `ipv4` or `inet4` for IPv4 \
  `ipv6` or `inet6` for IPv6

- _Protocol_ \
  `proto <protocol>` \
  `proto <number>` \
  where `<protocol>` is known protocol such as tcp, udp, icmp, icmp6, etc. or a valid
  protocol number: 6, 17 etc.

- _IP address_ \
  `[src|dst] ip <ipaddr>`  \
  `[src|dst] host <ipaddr>` \
  with `<ipaddr>` as any valid IPv4 or IPv6 address.  To check if an IP address is in a known IP
  list, use: \
  `[src|dst] ip in [ <iplist> ]` \
  `[src|dst] host in [ <iplist> ]` \
   where `<iplist>` is a space or comma separated list of individual `<ipaddr>`.

   IP addresses, networks, ports, AS number etc. can be specifically selected by using a direction
   qualifier, such as `src` or `dst`.

- _Network_ \
  `[src|dst] net a.b.c.d m.n.r.s` \
  Select the IPv4 network a.b.c.d with netmask m.n.r.s. \
  \
  `[src|dst] net <net>/<num>` \
  with `<net>` as a valid IPv4 or IPv6 network and `<num>` as mask bits.  The number of mask bits
  must match the  appropriate address family in IPv4 or IPv6. Networks may be abbreviated such
  as 172.16/16 if they are unambiguous.

- _Port_ \
  `[src|dst] port [comp] <num>` \
  with <num> as any valid port number.  If *comp* is omitted, '=' is assumed. \
  `[src|dst] port in [ <portlist> ]` \
  A port can be compared against a know list, where `<portlist>` is a space or comma separated
  list of individual port numbers.

- _Flags_ \
  `flags <tcpflags>` \
  with `<tcpflags>` as a combination of: \
     A - ACK \
     S - SYN \
     F - FIN \
     R - Reset  \
     P - Push   \
     U - Urgent \
     X -  All flags on \
  The  ordering  of  the  flags is not relevant. Flags not mentioned are treated as don't care.
  In order to get those flows with only the SYN flag set, use the
  syntax `flags S and not flags AFRPU`.

- _Packets_ \
  `packets [comp] <num> [scale]` \
  To filter for records with a specific packet count. \
  Example: `packets > 1k`

- _Bytes_ \
  `bytes [comp] <num> [scale]` \
  To filter for records with a specific byte count. \
  Example: `bytes 46` or `bytes > 100 and bytes < 200`

- _Packets per second_ (calculated value) \
  `pps [comp] num [scale]` \
  To filter for flows with specific packets per second.

- _Duration_ (calculate value) \
  `duration [comp] num` \
  To filter for flows with specific duration in milliseconds.

- _Bits per second_ (calculated value) \
  `bps [comp] num [scale]` \
  To filter for flows with specific bytes per second.

- _Bytes per packet_ (calculated value) \
  `bpp [comp] num [scale]` \
  To filter for flows with specific bytes per packet.

Following expressions are available only for processing live IPFIX records and therefore are
not supported by fdistdump.

- _Observation Domain ID (ODID)_ \
  `odid [comp] <number>` \
  To filter IPFIX records with a specific Observation Domain ID.

- _Exporter IP_ \
  `exporterip <ipaddr>` \
  `exporterip in [ <iplist> ]` \
  To filter for exporters connected with specified IP addresses.

- _Exporter port_ \
  `exporterport <port>` \
  `exporterport in [ <portlist> ]` \
  To filter for exporters connected with specified port.

- _Collector IP_ \
  `collectorip <ipaddr>` \
  `collectorip in [ <iplist> ]` \
  To filter for an input IP address of a running collector.

- _Collector port_ \
  `collectorport <port>` \
  `collectorport in [ <portlist> ]` \
  To filter for an input port of a running collector.

Instead of the identifiers above you can also use any IPFIX Information Element (IE) that is
supported by IPFIXcol. These IEs can be easily added to the configuration file of the collector
so even Private Enterprise IEs can be also used for filtering. See its manual page for more
information. Just keep in mind that these identifiers are not supported by fdistdump right now.

For example, IPFIX IE for the source port in the transport header is called "sourceTransportPort"
and essentially corresponds to the filter expression "src port". Therefore, the expressions
`sourceTransportPort 80` and `src port 80` represent the same filter.

### Filter examples

To dump all records of host 192.168.1.2: \
`ip 192.168.1.2`

To dump all record of network 172.16.0.0/16: \
`net 172.16.0.0/16`

To dump all port 80 IPv6 connections to any web server: \
`inet6 and proto tcp and ( src port > 1024 and dst port 80 )`

### Use-case example

Let's say that we would like to filter only POP3 flows. We know that ports of POP3 communication
are 110 (unencrypted) or 995 (encrypted). So we can write: \
`sourceTransportPort == 110 or sourceTransportPort == 995 or destinationTransportPort == 110 or destinationTransportPort == 995`

Instead of IPFIX IEs, we can use universal identifiers: \
`src port 110 or src port 995 or dst port 110 or dst port 995`

Source and destination port can be merged: \
`port 110 or port 995`

This expression still can be simplified: \
`port in [110, 995]`

All examples above represent the same filter.

[Back to Top](#top)
