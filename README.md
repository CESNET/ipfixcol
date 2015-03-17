#<a name="top"></a>IPFIXcol framework

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
6.  [Howto build](#build)
7.  [RPM](#rpm)
8.  [FastBit](#fastbit)
9.  [Contact us](#contact)
    *  [Reporting bugs](#bug)
    *  [Forum](#mailing)

##<a name="desc"></a> Framework description
IPFIXcol framework is a set of:

* **IPFIXcol** - collector for capturing IPFIX NetFlow data
* input, intermediate and storage **plugins** for collector
* **tools** for data processing etc.

##<a name="ipfixcol"></a> IPFIXcol
Described in it's [README](base/)

##<a name="plugins"></a> Plugins
IPFIX collector comes with several built-in plugins described at [IPFIXcol's page](base/).

There are also external plugins that are installed separately

###<a name="exin"></a> External input plugins
* **[nfdump](plugins/input/nfdump)** - NFDUMP file reader

###<a name="exmed"></a> External intermediate plugins
* **[geoip](plugins/intermediate/geoip)** - adds country codes into the metadata structure
* **[profiler](plugins/intermediate/profiler)** - fills metadata informations about profiles and channels
* **[profile_stats](plugins/intermediate/profile_stats)** - counts statistic per profile and channel
* **[stats](plugins/intermediate/stats)** - counts statistics per ODID

###<a name="exout"></a> External storage plugins
* **[fastbit](plugins/storage/fastbit)** - uses FastBit library to store and index data
* **[fastbit_compression](plugins/storage/fastbit_compression)** - uses FastBit library to store and index data with optional compression support
* **[json](plugins/storage/json)** - sends data in JSON format
* **[nfdump](plugins/storage/nfdump)** - stores data in NFDUMP file format
* **[postgres](plugins/storage/postgres)** - stores data into PostgreSQL database
* **[statistics](plugins/storage/statistics)** - uses RRD library to generate statistics for collected data
* **[unirec](plugins/storage/unirec)** - stores data in UniRec format

##<a name="btools"></a> Built-in tools
###ipfixviewer and ipfixconf
Destribed in IPFIXcol's [README](base/#tools)

##<a name="extools"></a> External tools

###<a name="fbitconvert"></a> fbitconvert
Converts data from NFDUMP file format into FastBit. Uses [IPFIXcol](base/), [nfdump input plugin](plugins/input/nfdump) and [fastbit storage plugin](plugins/storage/fastbit).

More info in it's [README](tools/fbitconvert/)

###<a name="fbitdump"></a> fbitdump

Tool for manipulating IPFIX data in FastBit database format. It uses FastBit library to read and index data.

More info in it's [README](tools/fbitdump/)

###<a name="fbitexpire"></a> fbitexpire

Daemon for removal old data.

More info in it's [README](tools/fbitexpire/)

###<a name="fbitmerge"></a> fbitmerge

Tool for merging FastBit data (saves free disk space, reduces number of files..)

More info in it's [README](tools/fbitmerge/)

##<a name="build"></a> How to build
Whole framework can be build at once with

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
```
to build all projects.

Or you can build each part (collector, tool(s), extarnal plugin(s)) separately.

The projects that depend on ipfixcol headers check the reltive path to base/header directory to use headers. 
When project is separated from the structure, it needs to have the headers installed (ipfixcol-devel package).

##<a name="rpm"></a> RPM
Each part of framework supports building rpm packages by running

```sh
make rpm
```

##<a name="fastbit"></a> FastBit
Plugins and tools that uses FastBit file format need FasBit library installed. IPFIXcol framework uses it's own fork of FastBit library to keep compatibility.

IPFIXcol's FastBit library can be found [here](https://github.com/CESNET/libfastbit).

##<a name="contact"></a> Contact us
###<a name="bug"></a> Reporting bugs

If you find any bug you can report it into [issue tracker](https://github.com/CESNET/ipfixcol/issues) here on GitHub.

###<a name="mailing"></a> Forum

if you have some questions or if you just want to share your ideas about useful features etc., please use [this forum](https://groups.google.com/forum/#!forum/ipfixcol).

[Back to Top](#top)
