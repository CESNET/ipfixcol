##<a name="top"></a>JSON storage plugin

###Plugin description
Plugin converts IPFIX data into JSON format.


###Format
Each data set is an array of data records.

Example:

```json
{
	"@type": "ipfix.entry",
	"ipfix.octetDeltaCount": 3970,
	"ipfix.packetDeltaCount": 14,
	"ipfix.flowStartMilliseconds": "2015-08-03T14:10:2.012",
	"ipfix.flowEndMilliseconds": "2015-08-03T14:11:2.380",
	"ipfix.ingressInterface": 2,
	"ipfix.ipVersion": 4,
	"ipfix.sourceIPv4Address": "52.24.214.8",
	"ipfix.destinationIPv4Address": "147.175.54.248",
	"ipfix.ipClassOfService": 0,
	"ipfix.ipTTL": 52,
	"ipfix.protocolIdentifier": "TCP",
	"ipfix.tcpControlBits": ".AP.SF",
	"ipfix.sourceTransportPort": 443,
	"ipfix.destinationTransportPort": 49285,
	"ipfix.egressInterface": 0,
	"ipfix.samplingInterval": 0,
	"ipfix.samplingAlgorithm": 0
}
```

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
		<metadata>no</metadata>
		<tcpFlags>formated</tcpFlags>
		<timestamp>formated</timestamp>
		<protocol>formated</protocol>

		<output>
			<type>print</type>
		</output>

		<output>
			<type>send</type>
			<ip>127.0.0.1</ip>
			<port>4444</port>
			<protocol>udp</protocol>
		</output>

		<output>
			<type>server</type>
			<port>4800</port>
			<blocking>yes</blocking>
		</output>
	</fileWriter>
</destination>
```
* **metadata** - Add record metadata to the output (yes/no) [default == no].
* **tcpFlags** - Convert TCP flags to formated style of dots and letters (formated) or to a number (raw) [default == raw].
* **timestamp** - Convert time to formated style (formated) or to a number (unix) [default == unix].
* **protocol** - Convert protocol identification to formated style (formated) or to a number (raw) [default == formated].
* **output** - Specifies JSON data processor. Multiple outputs are supported.
	* **type** - Output type. Actually only **print**, **send** and **server** are supported.
* **output:print** - Writes data to the standard output.
* **output:send** - Sends data over the network.
	* **ip** - IPv4/IPv6 address of remote host (default 127.0.0.1).
	* **port** - Remote port number (default 4739)
	* **protocol** - Connection protocol, one of UDP/TCP/SCTP (default UDP). This field is case insensitive.
* **output:server** - Sends data over the network to connected clients.
	* **port** - Local port number.
	* **blocking** - Type of the connection. Blocking (yes) or non-blocking (no).

[Back to Top](#top)
