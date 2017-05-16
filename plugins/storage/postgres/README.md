## <a name="top"></a>Posgres storage plugin
### Plugin description

Plugin stores IPFIX data into PostgreSQL database so it need postgresql library installed. Recommended PostgreSQL version is at least 8.4.9.

### Configuration

Default plugin configuration in **internalcfg.xml**:

```xml
<storagePlugin>
	<fileFormat>postgres</fileFormat>
	<file>/usr/share/ipfixcol/plugins/ipfixcol-postgres-output.so</file>
	<threadName>postgres</threadName>
</storagePlugin>
```

Or as `ipfixconf` output:

```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
        storage             postgres            postgres         /usr/share/ipfixcol/plugins/ipfixcol-postgres-output.so
```

Plugin needs configured information about database connection and user.

Example **startup.xml** configuration:

```xml
 <destination>
     <name>store data records in PostgreSQL database</name>
     <fileWriter>
          <fileFormat>postgres</fileFormat>
          <host>localhost</host>
          <hostaddr>localhost</hostaddr>
          <port>5432</port>
          <dbname>test</dbname>
          <user>username</user>
          <pass>password</pass>
     </fileWriter>
</destination>
```
*  **host** is name of host to connect to
*  **hostaddr** is IP address of host
*  **port** to connect to
*  **dbname** is name of database
*  **user** is name to use for connection
*  **pass** is password for authentication

[Back to Top](#top)
