## <a name="top"></a> NFDUMP input plugin

### Plugin description
Plugin reads nfcapd file and transforms it into IPFIX packet(s). It corresponds to nfdump version 1.6.x (headers are included within plugin)

### NFDUMP support
Plugin supports only nfdump extensions up to 25 and these types of records:
* CommonRecordV0Type
* ExtensionMapType

### Configuration

Default plugin configuration in **internalcfg.xml**

```xml
<inputPlugin>
    <name>nfdumpReader</name>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-nfdump-input.so</file>
    <processName>nfdumpFile</processName>
</inputPlugin>
```
Or as `ipfixconf` output:

```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
       input           nfdumpReader       nfdumpFile       /usr/share/ipfixcol/plugins/ipfixcol-nfdump-input.so
```

In **startup.xml** the only parameter to set is path to nfcapd file, e.g.:

```xml
<collectingProcess>
    <name>NFDUMP reader</name>
    <nfdumpReader>
        <file>file:/path/to/nfcapd.file</file>
    </nfdumpReader>
    <exportingProcess>ipfix file writer</exportingProcess>
</collectingProcess>
```

[Back to Top](#top)
