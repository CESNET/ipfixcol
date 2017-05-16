## <a name="top"></a>FastBit storage plugin
### Plugin description

The plugin uses FastBit library to store and index data.

### FastBit library

 IPFIXcol framework uses it's own fork of FastBit library to keep compatibility.

IPFIXcol's FastBit library can be found [here](https://github.com/CESNET/libfastbit).

### Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<storagePlugin>
	<fileFormat>fastbit</fileFormat>
	<file>/usr/share/ipfixcol/plugins/ipfixcol-fastbit-output.so</file>
	<threadName>fastbit</threadName>
</storagePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
        storage             fastbit            fastbit         /usr/share/ipfixcol/plugins/ipfixcol-fastbit-output.so
```

Example **startup.xml** configuration:

```xml
<destination>
     <name>store data records in FastBit database</name>
     <fileWriter>
          <fileFormat>fastbit</fileFormat>
          <path>storagePath/%o/%Y/%m/%d/</path>
          <dumpInterval>
               <timeWindow>300</timeWindow>
               <timeAlignment>yes</timeAlignment>
               <recordLimit>no</recordLimit>
               <bufferSize>50000</bufferSize>
          </dumpInterval>
          <namingStrategy>
               <type>time</type>
               <prefix>ic</prefix>
          </namingStrategy>
          <onTheFlyIndexes>yes</onTheFlyIndexes>
          <reorder>no</reorder>
          <indexes>
               <element enterprise = "0" id = "12"/>
               <element enterprise = "0" id = "8"/>
               <element id = "4"/>
          </indexes>
     </fileWriter>
</destination>
```

*  **path** specifies storage directory for collected data. See man pages (`man ipfixcol-fastbit-output`) for detailed informations about possible tokens.
*  **dumpInterval - timeWindow** is interval (in seconds) for rotation of data storage directory.
*  **dumpInterval - timeAlignment** turns on/off time alignment according to time window.
*  **dumpInterval - recordLimit** prevents data storage directory to become too huge.
*  **dumpInterval - bufferSize** specifies how many elements can be stored in buffer per row.
*  **namingStrategy - type** sets name asignment to data dumps (time/incremental/prefix).
*  **namingStrategy - prefix** specifies prefix to data dumps names.
*  **onTheFlyIndexes** tells plugin to create indexes for stored data. Elements for indexing can be specified so indexes are build only for those elements.
*  **reorder** tells plugin to reorder for stored data. Reorder is based on cardinality so queries on reordered data should be faster and data indexes smaller.

[Back to Top](#top)
