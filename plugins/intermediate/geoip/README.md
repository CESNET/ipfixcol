##<a name="top"></a>Profiler intermediate plugin
###Plugin description

This plugin fills informations about organizations and their profiles into the data record's metadata structure.

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<intermediatePlugin>
    <name>profiler</name>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-profiler-inter.so</file>
    <threadName>profiler</threadName>
</intermediatePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
     intermediate          profiler          profiler        /usr/share/ipfixcol/plugins/ipfixcol-profiler-inter.so
```

Example **startup.xml** configuration:

```xml
<profiler>
    <organization id="1">
        <rule id="1">
            <source>127.0.0.1</source>
            <odid>5</odid>
        </rule>
        <rule id="2">
            <source>127.0.0.1</source>
            <odid>3</odid>
            <dataFilter>ipVersion = 4 and exists tcpControlBits</dataFilter>
        </rule>
        <profile id="1">
            <filter>octetDeltaCount < 0xF</filter>
        </profile>
        <profile id="2">
            <filter>octetDeltaCount > 0xF or sourceTransportPort = 21</filter>
        </profile>
    </organization>
    <organization id="2">
        ...
    </organization>
</profiler>
```

*  **organization** specifies organization structure. ID is mandatory and must be unique according to other organizations.
*  **rule** specifies whether data record belongs to the organization. ID is mandatory and must be unique according to other rules within organization
*  **rule - source** is source IP address of exporter. Both IPv4 and IPv6 are supported.
*  **rule - odid** is Observation Domain ID set by exporting process.
*  **rule - dataFilter** is applied on data in data record. It uses the same syntax as filtering plugin (see its man pages for detailed description).
*  **profile** specifies one profile within organization. ID is mandatory.
*  **profile - filter** is applied on data in data record. Filter indicates whether the data record belongs to the profile. It uses the same syntax as filtering plugin (see its man pages for detailed description).

[Back to Top](#top)
