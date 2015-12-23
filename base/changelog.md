**Future release:**

**Version 0.9.0:**

* Many bug fixes
* New ipfix_elements API
* sFlow support is disable by default
* Fixed forwarding of UDP templates
* Unlimited number of incoming TCP connections
* Fixed statistics for multiple inputs with same ODID
* Fixed incorrect message when API version does not match
* Added option for single data manager (all ODIDs to single storage plugin)

**Version 0.8.1:**

*  Multiple destinations support in forwarding storage plugin
*  Improvements to anonymization intermediate plugin
*  Fixed sequence number gap detection
*  Fixed padding problem in NFv9 -> IPFIX conversion

**Version 0.8.0:**

*  Added Simple Socket library (libsiso)
*  Fixed default paths to configuration files
*  Added -e option for path to ipfix-elements.xml file
*  Added -S option for statistics about processed data and threads usage
*  New tool: ipfixsend
*  New intermediate plugin: odip
*  New intermediate plugin: hooks
*  Added metadata
*  New intermediate plugin: profiler
*  New storage plugin: json
*  New storage plugin: fastbit_compression
*  Added reconfiguration at runtime
*  New intermediate plugin: geoip
*  Added "terminating" flag accessible from plugins
*  New intermediate plugin: stats
*  New intermediate plugin: profile_stats
*  New intermediate plugin: uid
*  Added API version for plugins
