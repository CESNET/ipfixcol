##UDP-CPG input plugin

###Description
This plugin is inteded to be used in the environment of the high-availability cluster, where several instances of the IPFIXcol are running in the active/passive mode.
It works the same way as standard UDP input plugin, on top of that it uses closed process group to share template and option template sets with other IPFIXcol instances.
Without this feature, passive instances wouldn't receive templates and after active instance failure, new active instance would have to wait for periodic template sets.
That would mean data loss.

###Configuration
The collector must be configured to use UDP-CPG plugin in the **internalcfg.xml** configuration file.
You can do that either by manually editing the file or by `ipfixconf` tool:

```
ipfixconf add -c /path/to/internalcfg.xml -p i -n udp-cpg -t udp-cpg -s /path/to/ipfixcol-udp-cpg-input.so -f
```

```xml
<inputPlugin>
        <name>udp-cpg</name>
        <file>/usr/share/ipfixcol/plugins/ipfixcol-udp-cpg-input.so</file>
        <processName>udp-cpg</processName>
</inputPlugin>
```

The collector must be configured to use UDP-CPG plugin also in the **startup.xml** configuration file.
This configuration specifies which plugins are used by the collector to process data and provides configuration for the plugins themselves.
All configuration parameters for the UDP plugin can be also used for the UDP-CPG with the same semantics.
Only one parameter is new, the optional CPGName parameter which determines the name of the closed process group to use.
Any string is valid, but something like "ipfixcol" is the best option.
Without this parameter, no CPG is used and the plugin works as a standard UDP plugin.

```xml
<collectingProcess>
        <name>UDP-CPG collector</name>
        <udp-cpgCollector>
                <name>Listening port 4739</name>
                <localPort>4739</localPort>
                <CPGName>ipfixcol</CPGName>
        </udp-cpgCollector>
        <exportingProcess>forwarding export</exportingProcess>
</collectingProcess>
```
