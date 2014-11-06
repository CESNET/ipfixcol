##<a name="top"></a>JSON storage plugin

###Plugin description
Plugin converts IPFIX data into JSON format and sends them over the network.


###Format
Each data set is an array of data records.

Example:

```json
{
   "@type":"ipfix.entry",
   "ipfix":[
      {
         "destinationIPv4Address":"123.123.123.1",
         "destinationTransportPort":"8612",
         "e0id34":"0",
         "e0id35":"0",
         "egressInterface":"0",
         "flowEndMilliseconds":"2014-07-21T11:35:53.973",
         "flowStartMilliseconds":"2014-07-21T11:35:53.973",
         "ingressInterface":"0",
         "ipClassOfService":"0",
         "ipTTL":"64",
         "ipVersion":"4",
         "octetDeltaCount":"44",
         "packetDeltaCount":"1",
         "protocolIdentifier":"UDP",
         "sourceIPv4Address":"132.132.132.1",
         "sourceTransportPort":"58744"
      },
      {
         "destinationIPv4Address":"224.224.0.1",
         "destinationTransportPort":"8612",
         "e0id34":"0",
         "e0id35":"0",
         "egressInterface":"0",
         "flowEndMilliseconds":"2014-07-21T11:35:53.974",
         "flowStartMilliseconds":"2014-07-21T11:35:53.974",
         "ingressInterface":"0",
         "ipClassOfService":"0",
         "ipTTL":"1",
         "ipVersion":"4",
         "octetDeltaCount":"44",
         "packetDeltaCount":"1",
         "protocolIdentifier":"UDP",
         "sourceIPv4Address":"123.123.123.1",
         "sourceTransportPort":"51624"
      }
   ]
}


###Configuration

Default plugin configuration in **internalcfg.xml**

```xml
<storagePlugin>
    <fileFormat>json</fileFormat>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-json-output.so</file>
    <threadName>json</threadName>
</storagePlugin>
```
Or as `ipfixconf` output:

```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
       storage              json             json          /usr/share/ipfixcol/plugins/ipfixcol-json-output.so
```

Here is an example of configuration in **startup.xml**:

```xml
<destination>
    <name>JSON storage plugin</name>
      <fileWriter>
        <fileFormat>json</fileFormat>
	<ip>127.0.0.1</ip>
	<port>4739</port>
	<type>udp</type>
    </fileWriter>
</destination>
```

[Back to Top](#top)
