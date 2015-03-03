##<a name="top"></a>Stats intermediate plugin
###Plugin description

This plugin creates RRD databases per Observation Domain ID.

###Statistics data

Statistics are counted for number packets, traffic and flows by protocol type (total, udp, tcp, icmp, other).

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<intermediatePlugin>
    <name>stats</name>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-stats-inter.so</file>
    <threadName>stats</threadName>
</intermediatePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
     intermediate           stats            stats          /usr/share/ipfixcol/plugins/ipfixcol-stats-inter.so
```

Example **startup.xml** configuration:

```xml
<stats>
        <path>/path/to/RRDs</path>
        <interval>500</interval>
</stats>
```
*  **path** Path to folder where RRD files will be saved.
*  **interval** RRD update interval in seconds. Default value is 300.

[Back to Top](#top)
