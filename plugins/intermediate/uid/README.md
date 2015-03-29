##<a name="top"></a>User ID intermediate plugin
###Plugin description

Plugin uses sqlite3 database and fills user information according to source and destination address for each IPFIX data record.

####SQL database

SQL database file must contain table **logs** with these columns:
*  **id** - integer, primary key
*  **name** - string to be copied into the metadata structure
*  **ip** - IP address in text format
*  **action** - numerical value, **1** == login, **0** == logout
*  **time** - unix timestamp in seconds

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
	 intermediate            uid              uid           /usr/share/ipfixcol/plugins/ipfixcol-uid-inter.so
```

Example **startup.xml** configuration:

```xml
<uid>
	<path>/path/to/dbfile.db</path>
</uid>
```

*  **path** is path to the SQL database file.

[Back to Top](#top)
