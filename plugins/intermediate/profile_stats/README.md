##<a name="top"></a>profilestats intermediate plugin
###Plugin description

This plugin creates RRD databases per profile and channel.

###Statistics data

Statistics are counted for number packets, traffic and flows by protocol type (total, udp, tcp, icmp, other).

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

Example **startup.xml** configuration:

```xml
<profilestats>
        <path>/path/to/RRDs</path>
        <interval>500</interval>
</profilestats>
```
*  **path** Path to folder where RRD files will be saved.
*  **interval** RRD update interval in seconds. Default value is 300.

[Back to Top](#top)
