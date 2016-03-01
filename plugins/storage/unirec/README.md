##<a name="top"></a>UniRec storage plugin

###Table of Contents
1. [Plugin description](#descr)
2. [Configuration files](#conf)
  *  [IPFIXcol configuration file](#confipfix)
  *  [UniRec configuration file](#confuni)
3. [How to use](#howto)
4. [Compilation](#compile)
5. [Additional information](#info)


###<a name="descr"></a> Plugin description
UniRec plugin is storage plugin for IPFIX collector. It converts IPFIX fields, processed by IPFIX collector, to NEMEA UniRec format structure and sends them to specified TRAP interface.


###<a name="conf"></a> Configuration
####<a name="confipfix"></a> IPFIXcol configuration file
Default plugin configuration in **internalcfg.xml**

```xml
<storagePlugin>
  <fileFormat>unirec</fileFormat>
  <file>/usr/share/ipfixcol/plugins/ipfixcol-unirec-output.so</file>
  <threadName>unirec</threadName>
</storagePlugin>
```

Or as `ipfixconf` output

```
     Plugin type         Name/Format     Process/Thread         File        
 ----------------------------------------------------------------------------
        storage             unirec            unirec         /usr/share/ipfixcol/plugins/ipfixcol-unirec-output.so
```

Example configuration in **startup.xml** could look like this (this is only part for Unirec plugin):
```xml
<exportingProcess>
  <!-- Same name as in collectingProccess element -->
  <name>UniRec output</name>
  <destination>
    <name>Make unirec from the flow data</name>
    <fileWriter>
      <!-- Same name as in ipfixcol internalcfg.xml -->
      <fileFormat>unirec</fileFormat>
      <!-- Specify TRAP interface -->
      <interface>
        <!-- TRAP interface type. t is for TCP -->
        <type>t</type>
        <!-- TRAP interface port. UniRec flows will be sent to this port -->
        <params>8000</params>
        <!-- TRAP interface timeout. 0 is for TRAP_NO_WAIT (non-blocking) -->
        <ifcTimeout>0</ifcTimeout>
        <!-- TRAP interface flush timeout in micro seconds -->
        <flushTimeout>10000000</flushTimeout>
        <!-- TRAP interface buffer switch. 1 is for ON -->
        <bufferSwitch>1</bufferSwitch>
        <!-- Set merge method (joinflows or manager). Default is manager -->
        <ODIDGetMethod>manager</ODIDGetMethod>
        <!-- TRAP interface UniRec template -->
        <format>DST_IP,SRC_IP,BYTES,DST_PORT,SRC_PORT,PROTOCOL</format>
      </interface>
    </fileWriter>
  </destination>
</exportingProcess>
```

Unirec plugin can have as many **TRAP** interfaces as needed, but all elements in `interface` element are mandatory. Names of UniRec fields in `format` element are names from [UniRec configuration file](#confuni). 

Order in which to write these fields follows these rules:

1.  Largest fields come first.
2.  If two fields have same size, they need to be sorted alphabetically.
3.  Dynamic fields come last and are sorted alphabetically.

Only one instance of UniRec plugin can run at any given time. This means that UniRec plugin needs merged data at its input. To accomplish this, one and only one of the following method MUST be used:

1.  Using single data manager (-M switch when starting ipfixcol) [Default and preferred]
2.  Set up **joinflows intermediate plugin** (described [here](../../../base/README.md))

Example of simple configuration of **joinflows** plugin:

```xml
<!-- Intermediate plugins list -->
<intermediatePlugins>
  ...     
  <!-- Configuration for joinflows plugin -->
  <joinflows_ip>
     <!-- Set destination ODID -->
     <join to="63">
        <!-- Set source ODIDs for this dst ODID -->
        <from>*</from>
     </join>
  </joinflows_ip>
  ...
</intermediatePlugins>
```

This will merge all IPFIX messages with different ODID and send them to UniRec plugin. Number '63' must be different than any ODID that can possibly arrive to IPFIX collector.


####<a name="confuni"></a> UniRec configuration file

This file is loaded with UniRec plugin. It specifies which IPFIX field is mapped to which NEMEA UniRec field and size of given field. Every line corresponds to one UniRec field. 

For example IP address:

```
SRC_IP   16   e0id8   IPv4 address
```

* First column specify UniRec name. This could be anything but exact same name must be used in [IPFIX configuration file](#confipfix) in `format` element.

* Second column specifies size of UniRec field in bytes.

* Third column is mapping for IPFIX element. This means IPFIX element with enterprise ID 0 and ID 8 (which is source IP version 4) is converted to UniRec source IP.

* Fourth column is description. This column is mandatory.

To map more than one IPFIX element to one UniRec element this syntax can be used:

```
SRC_IP   16   e0id18,e0id27   IPv4 or IPv6 address
```

This will map IP version 4 or 6 to UniRec field.


###<a name="howto"></a> How to use

If everything is configured correctly, then just start ipfixcol binary and UniRec plugin will output data on specified port in NEMEA UniRec format.


###<a name="compile"></a> Compilation
No special compilation parameters are needed but for this plugin to work libtrap library needs to be installed on system.


###<a name="info"></a>Additional information
For additional information about TRAP or NEMEA go to this site: https://www.liberouter.org/technologies/nemea/

[Back to Top](#top)
