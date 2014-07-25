config_files = internalcfg.xml  ipfix-elements.xml  startup.xml

pkgsysconfdir = $(sysconfdir)/ipfixcol
pkgsysconf_DATA = $(config_files)

EXTRA_DIST = $(config_files) ipfixcol
CLEANFILES = internalcfg.xml

internalcfg.xml: internalcfg.xml.template
	sed 's|@pkgdatadir[@]|$(pkgdatadir)|g' $< > $@

