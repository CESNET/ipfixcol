##<a name="top"></a>FastBit storage plugin woth compression support
###Plugin description

The plugin uses FastBit library to store and index data and gzip or bzip2 libraries for compression.

The plugin was created by Jakub Adler as part of his [Bachelor's thesis](https://is.muni.cz/th/396111/fi_b/) at Masary University.

###FastBit library

 IPFIXcol framework uses it's own fork of FastBit library to keep compatibility.

IPFIXcol's FastBit library can be found [here](https://github.com/CESNET/libfastbit). Branch compression contains a fork of the library that allows to read compressed files.

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<storagePlugin>
	<fileFormat>fastbit_compression</fileFormat>
	<file>/usr/share/ipfixcol/plugins/ipfixcol-fastbit_compression-output.so</file>
	<threadName>fastbitc</threadName>
</storagePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
        storage  fastbit_compression           fastbitc         /usr/share/ipfixcol/plugins/ipfixcol-fastbit_compression-output.so
```

Example **startup.xml** configuration:

```xml
<destination>
    <name>store data records in FastBit database</name>
    <fileWriter>
        <fileFormat>fastbit_compression</fileFormat>
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
        <globalCompression>gzip</globalCompression>
        <compress>
            <template id="256">gzip</template>
            <element enterprise="0" id="27">gzip</element>
            <element enterprise="0" id="28">gzip</element>
        </compress>
        <compressOptions>
            <gzip>
                <level>9</level>
                <strategy>filtered</strategy>
            </gzip>
            <bzip2>
                <blockSize>1</blockSize>
                <workFactor>30</workFactor>
            </bzip2>
        </compressOptions>
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
*  **indexes** index creation can be defined for specific elements.
*  **globalCompression** turns on compression for all elements. Valid values are gzip and bzip2.
*  **compress** turns on compression for specific elements and/or templates. Valid values are gzip and bzip.
*  **compressOptions - gzip** Configures options for gzip compression.
*  **compressOptions - bzip2** Configures options for bzip2 compression.

[Back to Top](#top)
