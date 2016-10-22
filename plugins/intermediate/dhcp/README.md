##<a name="top"></a>DHCP intermediate plugin
###Plugin description

The plugin fills MAC addresses according to IP-MAC mapping stored in sqlite3 database.  
It can be used to set MAC addresses retrieved from DHCP log.  
Only IPv4 addresses are currently supported.  
MAC addresses for IP addresses not found in the database are set to zero.

####SQL database

SQL database file must contain table **dhcp** with these columns:
*  **timestamp**, integer, UNIX timestamp in seconds
*  **ip** - string, primary key
*  **mac** - string to be copied into the metadata structure

```
sqlite3 $DB_FILE 'CREATE TABLE IF NOT EXISTS dhcp '\
'(timestamp INTEGER NOT NULL, '\
'ip TEXT NOT NULL, '\
'mac TEXT NOT NULL, '\
'PRIMARY KEY (ip) ON CONFLICT REPLACE, '\
'UNIQUE (mac) ON CONFLICT REPLACE '\
');'
```

###Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<intermediatePlugin>
    <name>dhcp</name>
    <file>/usr/share/ipfixcol/plugins/ipfixcol-dhcp-inter.so</file>
    <threadName>dhcp</threadName>
</intermediatePlugin>
```

Or as `ipfixconf` output:
  
```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
	 intermediate            dhcp              dhcp           /usr/share/ipfixcol/plugins/ipfixcol-dhcp-inter.so
```

Example **startup.xml** configuration:

```xml
<dhcp>
	<path>/path/to/dbfile.db</path>
	<pair>
		<ip en="0" id="225"/>
		<mac en="0" id="81"/>
	</pair>
	<pair>
		<ip en="0" id="226"/>
		<mac en="0" id="57"/>
	</pair>
</dhcp>
```

*  **path** is path to the SQL database file.
*  **pair** is IP-MAC pair. MAC address for IP address from given elements is retrieved and substituted.
    *  **ip** IPv4 address element enterprise number and id.
    *  **mac** MAC address element enterprise number and id.

[Back to Top](#top)
