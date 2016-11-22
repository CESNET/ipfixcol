##<a name="top"></a>TimeNow intermediate plugin
###Plugin description

The plugin updates flowStartMilliseconds and flowEndMilliseconds elements in flows
to make the flow records up to date. The new time is computed as
```
old_time + (now - export_time)
```

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<intermediatePlugin>
    <name>timenow</name>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-timenow-inter.so</file>
    <threadName>timenow</threadName>
</intermediatePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
	 intermediate            timenow              timenow           /usr/share/ipfixcol/plugins/ipfixcol-timenow-inter.so
```

Example **startup.xml** configuration:

```xml
<timenow></timenow>
```

[Back to Top](#top)
