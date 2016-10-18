ACLOCAL_AMFLAGS = -I m4 

EXTRA_DIST = README.md LICENSE docker ansible

SUBDIRS = base \
	tools/fbitconvert \
	tools/fbitdump \
	tools/fbitmerge \
	tools/fbitexpire \
	tools/profilesdaemon \
	plugins/storage/fastbit \
	plugins/storage/fastbit_compression \
	plugins/storage/postgres \
	plugins/storage/json \
	plugins/storage/statistics \
	plugins/storage/nfdump \
	plugins/input/nfdump \
	plugins/intermediate/dhcp \
	plugins/intermediate/geoip \
	plugins/intermediate/profiler \
	plugins/intermediate/profile_stats \
	plugins/intermediate/stats \
	plugins/intermediate/uid

# plugins/storage/unirec is not added to SUBDIRS due to its dependency on libtrap
# plugins/storage/libnf is not added to SUBDIRS due to its dependency on libnf
# plugins/input/udp_cpg is not added to SUBDIRS due to its dependency on libcpg

