##<a name="top"></a>Profiler intermediate plugin
###Plugin description

This plugin fills informations about profiles and their channels into the data record's metadata structure.

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
	<profiles>/path/to/profiles.xml</profiles>
</profiler>
```

*  **profiles** is path to the file containing profiles specification.

####Profiles configuration

Example of **profiles.xml**:

```xml
<profile name="p1">

	<channel name="ch1">
		<sources>*</sources>
		<filter>ipVersion = 4</filter>
	</channel>

	<channel name="ch2">
		<sources>*</sources>
		<filter>odid != 5</filter>
	</channel>

	<channel name="ch3">
		<sources>*</sources>
		<filter>octetDeltaCount > 0xf</filter>
	</channel>

	<profile name="p2">

		<channel name="ch21">
			<sources>ch1, ch2</sources>
			<filter>sourceIPv4Address = 192.168.0.0/16</filter>
		</channel>

		<channel name="ch22">
			<sources>ch3</sources>
			<filter>...</filter>
		</channel>

	</profile>

	<profile name="p3">

		<channel name="ch31">
			<sources>*</sources>
			<filter>...</filter>
		</channel>

	</profile>

</profile>
```

*  **profile** is profile definition with options, channels and subprofiles.
*  **channel** specifies channel structure for profile's data filtering.
*  **sources** contains comma separated list of sources from which channel will receive data. Sources are channels from parent profile (except top level channels).
*  **filter** is filter applied on data records, specifying whether it belongs to the profile. It uses the same syntax as filtering intermediate plugin. Except data fields, profile filter can contain elements from IP and IPFIX header. Supported fields are: odid, srcaddr, dstaddr, srcport, dstport.

[Back to Top](#top)
