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
     Plugin type         Name/Format     Process/Thread    q     File        
 ----------------------------------------------------------------------------
     intermediate          profiler          profiler        /usr/share/ipfixcol/plugins/ipfixcol-profiler-inter.so
```

Example **startup.xml** configuration:

```xml
<collectingProcess>
	...
	<profiles>/path/to/profiles.xml</profiles>
</collectingProcess>
...
<intermediatePlugins>
	<profiler>
	</profiler>
	...
</intermediatePlugins>
```

*  **profiles** is path to the file containing profiles specification.

####Profiles configuration

Example of **profiles.xml**:

```xml
<profile name="live">
	<type>normal</type>
	<directory>/some/directory/</directory>

	<channelList>
		<channel name="ch1">
			<sourceList>
				<source>*</source>
			</sourceList>
			<filter>ipVersion = 4</filter>
		</channel>
		<channel name="ch2">
			<sourceList>
				<source>*</source>
			</sourceList>
			<filter>odid != 5</filter>
		</channel>
	</channelList>

	<subprofileList>
		<profile name="p1">
			<type>normal</type>
			<directory>/some/directory/p1/</directory>

			<channelList>
				<channel name="ch11">
					<sourceList>
						<source>ch1</source>
						<source>ch2</source>
					</sourceList>
					<filter>sourceIPv4Address = 192.168.0.0/16</filter>
				</channel>
				<channel name="ch12">
					<sourceList>
						<source>ch1</source>
					</sourceList>
					<filter>sourceTransportPort == 25</filter>
				</channel>
			</channelList>
		</profile>

		<!-- other subprofiles -->
	</subprofileList>

</profile>
```

*  **profile** - Profile definition with options, channels and subprofiles.

	* **type** - Specifies the type of a profile - normal/shadow. _normal_ profile means that IPFIXcol plugins should store all valuable data.  _shadow_ profile means that IPFIXcol plugins should store only statistics.
	* **directory** - Directory for data store of valuable data and statistics. Must be unique for each profile.
	* **channelList** - List of channels that belong to the profile. At least one channel must be specified. A number of channels are unlimited.
	* **subprofileList** - List of subprofiles that belong to the profile. This item is optional. A number of subprofiles are unlimited.

*  **channel** - Channel structure for profile's data filtering.
	*  **sourceList** - List of sources from which channel will receive data. Sources are channels from parent's profile (except top level channels). If a profile receive data from all parent's channels only one source with '\*' can by used. _shadow_ profiles must always use only '\*' source!
	*  **filter** - Filter applied on data records, specifying whether it belongs to the profile. It uses the same syntax as filtering intermediate plugin. Except data fields, profile filter can contain elements from IP and IPFIX header. Supported fields are: odid, srcaddr, dstaddr, srcport, dstport.

[Back to Top](#top)
