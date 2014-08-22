
MANSRC = fbitdump.dbk fbitdump.xml.dbk fbitdump-plugins.dbk

EXTRA_DIST = $(MANSRC)

man_MANS = fbitdump.1 fbitdump.xml.5 fbitdump-plugins.7

CLEANFILES = fbitdump.1 fbitdump.xml.5 fbitdump-plugins.7

%.1 %.5 : %.dbk
	@if [ -n "$(XSLTPROC)" ]; then \
		if [ -f "$(XSLTMANSTYLE)" ]; then \
			echo $(XSLTPROC) $(XSLTMANSTYLE) $<; \
			$(XSLTPROC) $(XSLTMANSTYLE) $<; \
		else \
			echo "Missing $(XSLTMANSTYLE)!"; \
			exit 1; \
		fi \
	else \
		echo "Missing xsltproc"; \
	fi

