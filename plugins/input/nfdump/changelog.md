**Future release:**

**Version 0.2.3:**

*  Fixed markdown syntax
*  Support DocBook XSL Stylesheets v1.79
*  Fixed problem with plugin update removing plugin configuration.
*  Added info about used headers to configure script of all plugins
*  Updated build system to work with Fedora copr build system (epel7 and all fedora releases)

**Version 0.2.1:**

*  Fixed several problems reported by Coverity Scan

**Version 0.2.0:**

*  Support for new extensions (id: 26, 27)
*  Fixed bug where bad template was used when template ID was 256
*  Fixed bug with processing big files with multiple data blocks
*  Support for record structure `common_record_s`
*  Support for decompression of data blocks

*  Fixed sizes of IPFIX elements
*  Using `struct common_record_v0_s` when working with `CommonV0Record`
