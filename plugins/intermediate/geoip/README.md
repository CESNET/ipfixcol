##<a name="top"></a>GeoIP intermediate plugin
###Plugin description

This plugin fills informations about country codes of source and destination address into the data record's metadata structure.

###Geolocation

For geolocation, MaxMind GeoIP API and database is used.

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<intermediatePlugin>
    <name>geoip</name>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-geoip-inter.so</file>
    <threadName>geoip</threadName>
</intermediatePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
     intermediate          geoip          geoip        /usr/share/ipfixcol/plugins/ipfixcol-geoip-inter.so
```

Example **startup.xml** configuration:

```xml
<geoip>
	<path>/path/to/GeoIP.dat</path>
	<path6>/path/to/GeoIPv6.dat</path6>
</geoip>
```

*  **path** (optional) is a path to IPv4 database file. By default, file from installed GeoIP package is used.
*  **path6** (optional) is a path to IPv6 database file. By default, GeoIPv6.dat distributed with plugin is used.

[Back to Top](#top)
