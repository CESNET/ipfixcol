##<a name="top"></a>statistics storage plugin

###Plugin description
Plugin calculates statistics from IPFIX data. It uses RRD database.

###Calculated data
Statistics are calculated for

* bytes
* packets
* flows

###Configuration

Default plugin configuration in **internalcfg.xml**

```xml
<storagePlugin>
    <fileFormat>statistics</fileFormat>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-statistics-output.so</file>
    <threadName>statistics</threadName>
</storagePlugin>
```
Or as `ipfixconf` output:

```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
       storage           statistics        statistics       /usr/share/ipfixcol/plugins/ipfixcol-statistics-output.so
```

Here is an example of configuration in **startup.xml**:

```xml
<destination>
    <name>make statistics from the flow data</name>
      <fileWriter>
        <fileFormat>statistics</fileFormat>
        <file>/patth/to/rrd_file</file>
        <interval>300</interval>
    </fileWriter>
</destination>
```
* **file** is a path to RRD database file for writing
* **interval** is time interval of flow data to compute statistics for

[Back to Top](#top)
