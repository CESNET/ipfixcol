# <a name="top"></a>IPFIXcol framework

<!--
[![Build Status](https://travis-ci.org/CESNET/ipfixcol.svg?branch=coverity_scan)](https://travis-ci.org/CESNET/ipfixcol)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/5119/badge.svg)](https://scan.coverity.com/projects/cesnet-ipfixcol)
-->

## Table of Contents
1.  [Framework description](#desc)
2.  [IPFIXcol](#ipfixcol)
3.  [Plugins](#plugins)
    *  [External input plugins](#exin)
    *  [External intermediate plugins](#exmed)
    *  [External storage plugins](#exout)
4.  [Built-in tools](#btools)
5.  [External tools](#extools)
    *  [fbitconvert](#fbitconvert)
    *  [fbitdump](#fbitdump)
    *  [fbitexpire](#fbitexpire)
    *  [fbitmerge](#fbitmerge)
	*  [profilesdaemon](#profilesdaemon)
6.  [Howto install](#install)
7.  [Howto build](#build)
8.  [Docker](#docker)
9.  [RPM](#rpm)
10.  [FastBit](#fastbit)
11.  [Contact us](#contact)
    *  [Reporting bugs](#bug)
    *  [Forum](#mailing)

## <a name="desc"></a> Framework description
IPFIXcol framework is a set of:

* **IPFIXcol** - collector for capturing IPFIX NetFlow data
* input, intermediate and storage **plugins** for collector
* **tools** for data processing etc.

To generate data for the IPFIXcol, look at the list [list of supported flow exporters](https://github.com/CESNET/ipfixcol/wiki/Supported-Flow-Exporters).

## <a name="ipfixcol"></a> IPFIXcol
Described in it's [README](base/)

## <a name="plugins"></a> Plugins
IPFIX collector comes with several built-in plugins described at [IPFIXcol's page](base/).

There are also external plugins that are installed separately

### <a name="exin"></a> External input plugins
* **[nfdump](plugins/input/nfdump)** - NFDUMP file reader

### <a name="exmed"></a> External intermediate plugins
* **[geoip](plugins/intermediate/geoip)** - adds country codes into the metadata structure
* **[profiler](plugins/intermediate/profiler)** - fills metadata informations about profiles and channels
* **[profile_stats](plugins/intermediate/profile_stats)** - counts statistic per profile and channel
* **[stats](plugins/intermediate/stats)** - counts statistics per ODID
* **[uid](plugins/intermediate/uid)** - fills user identity information

### <a name="exout"></a> External storage plugins
* **[fastbit](plugins/storage/fastbit)** - uses FastBit library to store and index data
* **[fastbit_compression](plugins/storage/fastbit_compression)** - uses FastBit library to store and index data with optional compression support
* **[json](plugins/storage/json)** - converts data into JSON format
* **[nfdump](plugins/storage/nfdump)** - stores data in NFDUMP file format
* **[postgres](plugins/storage/postgres)** - stores data into PostgreSQL database
* **[statistics](plugins/storage/statistics)** - uses RRD library to generate statistics for collected data
* **[unirec](plugins/storage/unirec)** - stores data in UniRec format

## <a name="btools"></a> Built-in tools
### ipfixviewer and ipfixconf
Destribed in IPFIXcol's [README](base/#tools)

## <a name="extools"></a> External tools

### <a name="fbitconvert"></a> fbitconvert
Converts data from NFDUMP file format into FastBit. Uses [IPFIXcol](base/), [nfdump input plugin](plugins/input/nfdump) and [fastbit storage plugin](plugins/storage/fastbit).

More info in it's [README](tools/fbitconvert/)

### <a name="fbitdump"></a> fbitdump

Tool for manipulating IPFIX data in FastBit database format. It uses FastBit library to read and index data.

More info in it's [README](tools/fbitdump/)

### <a name="fbitexpire"></a> fbitexpire

Daemon for removing old data.

More info in it's [README](tools/fbitexpire/)

### <a name="fbitmerge"></a> fbitmerge

Tool for merging FastBit data (saves free disk space, reduces number of files..)

More info in it's [README](tools/fbitmerge/)

### <a name="profilesdaemon"></a> profilesdaemon

Tool for profiles management and distribution

[More info](tools/profilesdaemon/)

## <a name="install"></a> How to install

Individual packages of the IPFIXcol framework can be installed from [Fedora copr repository](https://copr.fedorainfracloud.org/coprs/g/CESNET/IPFIXcol/)
Just add the repository to your system:
```
dnf copr enable @CESNET/IPFIXcol 
```

And install the packages you need (e.g. IPFIXcol framework and JSON output plugin):
```
dnf install ipfixcol ipfixcol-json-output
```

If you not are using one of the supported operating systems, you can [build the IPFIXcol from sources](#build).

## <a name="build"></a> How to build
Dependencies must be installed first. For Fedora, CentOS and RHEL the list of necessary packages is as follows:
```
autoconf bison docbook-style-xsl doxygen flex 
gcc gcc-c++ git libtool libxml2 libxml2-devel 
libxslt lksctp-tools-devel lzo-devel make 
openssl-devel GeoIP-devel rrdtool-devel
sqlite-devel postgresql-devel corosync corosync-devel rpm-build
```

Debian and Ubuntu distributions have a different names for some of the packages:
```
autoconf bison build-essential docbook-xsl doxygen flex
git liblzo2-dev libtool libsctp-dev libssl-dev libxml2
libxml2-dev pkg-config xsltproc libgeoip-dev librrd-dev
libsqlite3-dev libpq-dev libcpg-dev corosync-dev
```

IPFIXcol does not support openssl1.1, therefore you need to use libssl1.0-dev on Debian Jessie.

Moreover, you need to build the [FastBit library](#fastbit)

First, download IPFIXcol git repository (do NOT forget to use `--recursive` option):
```sh
git clone --recursive https://github.com/CESNET/ipfixcol.git
```

Note: If you have updated from a previous revision of the repository without a submodule 
or if you forgot to add `--recursive` option, you can just download the submodule manually:
```sh
git submodule update --init --recursive
```

After installing all dependencies and downloading the repository, the whole framework can be build at once with

```sh
autoreconf -i 
```
to generate configure script from configure.ac, Makefile.in from Makefile.am and install missing files.

```sh
./configure
```
to configure packages in subdirectories and generate Makefiles. 

```sh
make
make install
```
to build and install all projects.

Or you can build each part (collector, tool(s), external plugin(s)) separately.

The projects that depend on ipfixcol headers check the reltive path to base/header directory to use headers. 
When project is separated from the structure, it needs to have the headers installed (ipfixcol-devel package).

## <a name="docker"></a> Docker

IPFIXcol can be used with Docker. See [Docker howto](docker).

## <a name="ansible"></a> Ansible

IPFIXcol can also be installed using Ansible orchestration. See [Ansible howto](ansible).

## <a name="rpm"></a> RPM
Each part of framework supports building rpm packages by running

```sh
make rpm
```
RPMs can be build only for specific parts, not the whole project.

## <a name="fastbit"></a> FastBit
Plugins and tools that uses FastBit file format need FasBit library installed. IPFIXcol framework uses it's own fork of FastBit library to keep compatibility.

IPFIXcol's FastBit library can be found [here](https://github.com/CESNET/libfastbit).

## <a name="contact"></a> Contact us
### <a name="bug"></a> Reporting bugs

If you find any bug you can report it into [issue tracker](https://github.com/CESNET/ipfixcol/issues) here on GitHub.

### <a name="contribution"></a> Contributing to IPFIXcol

We are open to contributions to IPFIXcol which improve the stability and functionality of the collector. To keep the code readable and consistent, please adhere to the [coding style](coding_style.md) document. 

### <a name="mailing"></a> Forum

if you have some questions or if you just want to share your ideas about useful features etc., please use [this forum](https://groups.google.com/forum/#!forum/ipfixcol).

[Back to Top](#top)
