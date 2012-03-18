IPFIXcol dummy storage plugin

This plugin can be used to test the IPFIXcol throughput. It does not store any data, only sleeps for defined time in store_packet function.

To use this plugin, use following configuration:
In /etc/ipfixcol/internalcfg.xml
	<storagePlugin>
		<fileFormat>dummy</fileFormat>
		<file>/path/to/ipfixcol-dummy-output.so</file>
	</storagePlugin>

In /etc/ipfixcol/startup.xml
	<destination>
		<fileWriter>
			<fileFormat>dummy</fileFormat>
			<delay>2500</delay>
		</fileWriter>
	</destination>

Set the <delay> element to number of microseconds that the plugin should spend in store_packet function.
