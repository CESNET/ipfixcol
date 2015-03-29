static_config_files = ipfix-elements.xml  startup.xml
generated_config_files = internalcfg.xml

config_files = $(generated_config_files) $(static_config_files)

pkgsysconfdir = $(sysconfdir)/ipfixcol
pkgsysconf_DATA = $(config_files)

EXTRA_DIST = $(static_config_files) ipfixcol ipfixcol-check-running internalcfg.xml.template
CLEANFILES = $(generated_config_files)

internalcfg.xml: internalcfg.xml.template
	sed 's|@pkgdatadir[@]|$(pkgdatadir)|g' $< > $@

