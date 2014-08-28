##<a name="top"></a>NFDUMP storage plugin
###Plugin description

Plugin stores IPFIX data into nfcapd file format.  

###NFDUMP support

NFDUMP file format is designed to store only certain flow information. So this plug-in can´t store all IPFIX elements to NFDUMP files. There are few elements which **MUST** be present in IPFIX record otherwise WHOLE record is ignored and NOT stored at all(this usually means all data records for certain template). Rest of supported IPFIX elements is optional. IPFIX records can contain unsupported elements too (these elements are simply ignored). See below for supported IPFIX elements (identified by numbers assigned by IANA).

You can find list of necessary/optional elements in plugins's man pages.

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<storagePlugin>
	<fileFormat>nfdump</fileFormat>
	<file>/usr/share/ipfixcol/plugins/ipfixcol-nfdump-output.so</file>
	<threadName>nfdump</threadName>
</storagePlugin>
```

Or as `ipfixconf` output:

```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
        storage             nfdump            nfdump         /usr/share/ipfixcol/plugins/ipfixcol-unirec-output.so
```

Example **startup.xml** configuration:

```xml
<destination>
   <name>store data records in NFDUMP file format</name>
   <fileWriter>
        <fileFormat>nfdump</fileFormat>
        <path>storagePath/%o/%Y/%m/%d/</path>
        <prefix>nfcapd.</prefix>
        <ident>file ident</ident>
        <compression>yes</compression>
        <dumpInterval>
             <timeWindow>300</timeWindow>
             <timeAlignment>yes</timeAlignment>
             <bufferSize>50000</bufferSize>
        </dumpInterval>
   </fileWriter>
</destination>
```

*  **path** is path to store data (see man pages for detailed info)
*  **prefix** specifies name prefix for output files
*  **ident** specifies name identification line for nfdump files
*  **compression** turns on/off LZO compression
*  **dumpInterval - timeWindow** is interval for rotation of nfdump files in seconds
*  **dumpInterval - timeAlignment** turns on/off time alignment according to **timeWindow**
*  **dumpInterval - bufferSize** specifies size of internal buffer in bytes

[Back to Top](#top)
