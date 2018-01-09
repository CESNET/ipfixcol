**Future release:**

**Version 0.4.3:**
* Fixed configuration for CESNET SIP plugin

**Version 0.4.2:**
* Fixed markdown syntax
* Support DocBook XSL Stylesheets v1.79
* Removed pkgconfig from spec file dependencies.
* fix and improve coding style
* fix vertical spacing
* Updated build system to work with Fedora copr build system (epel7 and all fedora releases)

**Version 0.4.1:**

*  Fixed documentation of -t parameter
*  Updated pugixml for fbitdump
*  Fixed build with latest Fastbit library
*  Add text IE to arg for plugins

**Version 0.4.0:**

*  Added parsing of plugin values in bit operations (&, |)
*  Fixed parsing of filter strings using plugins
*  Fixed filter problems with unsigned long values
*  Fixed problem with -t parameter
*  Sorting column is required in output format

*  Using aliases in SELECT clause to identify columns
*  Computed columns are now evaluated in fastbit library
*  Summary columns must have specified summary type (sum or avg)
*  Fixed computing bps, bpp etc.
*  Fixed printing percentages (only for "sum" summary columns)
*  Supported ordering by computed columns (bps...)
*  %fl column is not required in output format
*  Added post-aggregate filter (option -P)
*  Added filter check option (-Z)

