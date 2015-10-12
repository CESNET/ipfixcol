##<a name="top"></a>Table of Contents

1.  [What is IPFIXcol](#what)
2.  [Configuration](#conf)
    *  [Reconfiguration](#reconf)
3.  [Built-in plugins](#bplugs)
    *  [Input plugins](#input)
    *  [Intermediate plugins](#inter)
    *  [Storage plugins](#storage)
4.  [External plugins](#explugs)
5.  [Other tools](#tools)
    *  [ipfixviewer](#view)
    *  [ipfixconf](#ipfixconf)
6.  [How to build](#howbuild)
7.  [How to run](#howrun)
8.  [How to write plugins](#howplug)

##<a name="what"></a>What is IPFIXcol

IPFIXcol is a flexible IPFIX flow data collector designed to be easily extensible by plugins.

It loads input, intermediate and output plugins on startup. Each input plugin runs in a different process.

IPFIXcol corresponds to [RFC7011](http://tools.ietf.org/html/rfc7011)

![IPFIXcol](documentation/img/arch_scheme.png)

##<a name="conf"></a>Configuration

IPFIXcol stores its configuration in the **/etc/ipfixcol/** directory.


* **ipfix-elements.xml** contains a description of the known IPFIX elements assigned by IANA (http://www.iana.org/assignments/ipfix/ipfix.xml).

* **internalcfg.xml** contains configuration of plugins used in startup.xml. Can be viewed/edited with **ipfixconf** tool.

* **startup.xml** describes how IPFIXcol is configured at startup, which plugins are used and where the data will be stored. The XML is self-documented, so read the elements description carefully. The collector will listen on TCP, UDP and SCTP on startup by default, so be careful to configure which plugin you want to use before starting it.

###<a name="reconf"></a>Reconfiguration

Collector can be reconfigured at runtime by sending SIGUSR1 signal. When this signal is received, startup configuration is reloaded and chages are processed.

Reconfiguration can:

*  Add/remove intermediate plugin(s)
*  Add/remove storage plugin(s)
*  Add/remove collecting process
*  Change exporting process
*  Change plugin settings (plugin is reloaded)
*  Reorder intermediate plugins (they're removed and loaded in the new order)

##<a name="bplugs"></a>Built-in plugins
###<a name="input"></a>Input plugins

* **TCP**, **UDP** and **SCTP** plugin are provided accept data from the network. Each can be configured to listen on a specific interface and a port. They are compatible with IPFIX, Netflow v5, Netflow v9 and sFlow.

* **IPFIX file** format input plugin can read data from a file in the mentioned format and store them in any other, depending on the storage plugin used. The IPFIX file format is specified in [RFC5655](http://tools.ietf.org/html/rfc5655).

###<a name="inter"></a>Intermediate plugins

* **anonymization** - anonymizes IP addresses with Crypto-PAn algorithm.

* **dummy** only passed message to next plugin.

* **filter** goes through data records and filters them according to profiles set in **startup.xml**. User can specify profiles with source Observation Domain IDs (ODID), output ODID and filter string.

* **joinflows** plugin merges multiple flows into one and adds information about original ODID to each Template and Data record.

###<a name="storage"></a>Storage plugins

By default, Output manager dynamically creates for each ODID an instance of Data manager with private instances of storage plugins. This can be useful, for example, when you want to store flows from different ODIDs into different files.
If you don't need to have different storage plugins for every ODID, you can enable single Data manager in **startup.xml** by adding `<singleManager>yes</singleManager>` to particular exporting process.

* **IPFIX file** format storage plugin stores data in the IPFIX format in flat files. The storage path must be configured in **startup.xml** to determine where to store the data.

* **ipfixviewer** storage plugin displays captured ipfix data (doesn't store them).

* **dummy** storage plugin does not store any data. It is available for collector performance testing purposes only. The time that plugins spends in _store_packet_ function can be configured.

* **forwarding** plugin sends data over the network (e.g. to the next collector). There is configurable connection type (TCP, UDP or SCTP), destination port and IPv4 or IPv6 address. With UDP, template refresh time etc. can be set.

##<a name="explugs"></a>External plugins
External plugins are described in the main [README](https://github.com/CESNET/ipfixcol#plugins).

##<a name="tools"></a> Other tools
###<a name="view"></a>ipfixviewer
A tool for displaying captured ipfix data. Uses IPFIXcol, IPFIX file input plugin and ipfixviewer storage plugin.

###<a name="ipfixconf"></a>ipfixconf
This tool provides interface to list, add and remove plugins from internal configuration so you don't need to edit XML file manualy. Each external plugin uses this tool after succesfull installation.

##<a name="howbuild"></a>How to build
Simply with:

```sh
autoreconf -i
./configure
make
```

And possibly:
```sh
sudo make install
```

##<a name="howrun"></a>How to run
###Start ipfixcol
The simplest way how to run collector is just running
```sh
ipfixcol
```
It uses default configuration at /etc/ipfixcol/startup.xml.

Use `-v` option to specify the level of verbosity of the collector. 
```sh
ipfixcol -v 3
```

To run the collector as a daemon with specified startup configuration, use
```sh
ipfixcol -d -c path/to/startup.xml
```

###Stop collector

IPFIXcol catches SIGINT, SIGQUIT and SIGTERM signals. When multiple input plugins are used, it is forked to multiple processes. You can stop each instance separately according to used input plugin.

For example:
```sh
killall ipfixcol:UDP
killall ipfixcol:TCP
...
```

##<a name="howplug"></a>How to write plugins

See [wiki page](https://github.com/CESNET/ipfixcol/wiki/How-to-write-IPFIXcol-plugins).

[Back to Top](#top)
