## <a name="top"></a>Lnfstore storage plugin

### Plugin description
The lnfstore plugin converts and store IPFIX data into NfDump files. Only a subset of IPFIX elements that have NetFlow equivalents can be stored into NfDump files. Other elements are skipped.

### Dependencies

* [Libnf](https://github.com/VUTBR/libnf) (C interface for processing nfdump files)
* [Bloom Filter Indexes](https://github.com/CESNET/bloom-filter-index)

### Configuration

Default plugin configuration in **internalcfg.xml**

```xml
<storagePlugin>
	<fileFormat>lnfstore</fileFormat>
	<file>/usr/share/ipfixcol/plugins/ipfixcol-lnfstore-output.so</file>
	<threadName>lnfstore</threadName>
</storagePlugin>
```
Or as `ipfixconf` output:

```
     Plugin type         Name/Format     Process/Thread         File
 ----------------------------------------------------------------------------
       storage             lnfstore         lnfstore          /usr/share/ipfixcol/plugins/ipfixcol-lnfstore-output.so
```

Here is an example of configuration in **startup.xml**:

```xml
<destination>
	<name>Storage</name>
	<fileWriter>
		<fileFormat>lnfstore</fileFormat>
		<profiles>no</profiles>
		<storagePath>/tmp/IPFIXcol/lnfstore</storagePath>
		<identificatorField>ipfixcol</identificatorField>
		<compress>yes</compress>
		<dumpInterval>
			<timeWindow>300</timeWindow>
			<align>yes</align>
		</dumpInterval>
		<index>
			<enable>yes</enable>
			<autosize>yes</autosize>
		</index>
	</fileWriter>
</destination>

<!--## Only one plugin for all ODIDs -->
<singleManager>yes</singleManager>
```

We strongly recommend to use this plugin with activated Single Manager mode
i.e. only one instance of the plugin is shared among all ODIDs. See the
example configuration.

In normal mode, files will be stored based on a configuration using
the following template: *\<storagePath\>/YYYY/MM/DD/\<prefix\>\<suffixMask\>*
where "YYYY/MM/DD" means year/month/day. However, when profile mode is enabled
flows will stored into directories of all channels where they belong. In this
case the template is slightly different:
*`profileDir`/channels/`channelName`/YYYY/MM/DD/\<prefix\>\<suffixMask\>*,
where `profileDir` and `channelName` are parameters from a profiling
configuration.

To speed up search of records of an IP address in multiple data files, the
plugin can also create index files. These files will be create
simultaneously with data files and they can be utilized by tools such as
*fdistdump* to promptly determine if there is at least one record with the
specified IP address in a file. This can dramatically reduce the number of
processed files and provide query results faster.

Parameters:

* **profiles** - When it is enabled ("yes"), flows will be stored into
directories of profiles defined by the profiler intermediate plugin (default: no).

* **storagePath** - The path element specifies the storage directory for data
files. This path must already exist in your system. Otherwise all data will
be lost. If profile storage is enabled and this element is defined then the
plugin makes sure that profiles will be stored only into subdirectories in
this path. In other words, profiles with storage directories outside of the
path are omitted. This allows you to make sure that files will be stored
only into specified location. This element can be also omitted if profile
storage is enabled but no directory check will be performed.
Path may contain special character sequences, each of which is
introduced by a "%" character and terminated by some other character.
Each of this sequences is substituted by its value. Currently supported special
characters: %h = hostname.

* **prefix** - Specifies the first part of output file names (default "lnf.").

* **suffixMask** - Specifies name suffix of output files. The mask can contain
format specifier for day, month, etc. This allows you to create names based
on format specifiers (default: "%Y%m%d%H%M%S"). See *strftime* for all
specifiers.

* **identificatorField** - Specifies name identification line of nfdump files.

* **compress** - Enable/disable LZO compression for files (yes/no) (default: no).

* **dumpInterval**
	* **timeWindow** - Specifies the time interval in seconds to rotate files
		(default: 300).
	* **align** - Align file rotation with next N minute interval (yes/no)
		(default: yes).

* **index**
	* **enable** - Enable/disable creation of Bloom Filter indexes (yes/no)
		(default: no).

	* **autosize** - Enable/disable automatic resize of index files based on
		the number of unique IP addresses in the last dump interval (yes/no)
		(default: yes).

	* **prefix** - Specifies the first part of output file names (default: "bfi.").

	* **estimatedItemCount** - Expected number of unique IP addresses in dump
		interval. When *autosize* is enabled this value is continuously
		recalculated to suit current utilization.
		The value affects the size of index files i.e. higher value, larger
		files (default: 100000).

	* **falsePositiveProbability** - False positive probability of the index.
		The probability that presence test of an IP address indicates that
		the IP address is present in a data file, when it actually is not.
		It does not affect the situation when the IP address is actually in
		the data file i.e. when the IP is in the file, the result of the test
		is always correct. The value affects the size of index files i.e.
		smaller value, larger files (default: 0.01).

[Back to Top](#top)
