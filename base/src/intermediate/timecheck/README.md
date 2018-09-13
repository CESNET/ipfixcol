## <a name="top"></a>TimeCheck intermediate plugin
### Plugin description

The plugin checks that flowStartMilliseconds and flowEndMilliseconds elements in flows
are relatively recent. It reports flows from the future (more than one day from current time).

### Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<intermediatePlugin>
    <name>timecheck</name>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-timecheck-inter.so</file>
    <threadName>timecheck</threadName>
</intermediatePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
	 intermediate            timecheck              timecheck           /usr/share/ipfixcol/plugins/ipfixcol-timecheck-inter.so
```

Example **startup.xml** configuration:

```xml
<timecheck></timecheck>
```

[Back to Top](#top)
