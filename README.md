#IPFIXcol framework

IPFIXcol framework is a set of:

* **IPFIXcol** - collector for capturing IPFIX NetFlow data
* input, intermediate and storage **plugins** for collector
* **tools** for data processing etc.

##IPFIXcol
Described in it's [README](base/README.md)

##Plugins
IPFIX collector comes with several built-in plugins described at [IPFIXcol's page](base/README.md).

There are also external plugins that are installed separately

###External input plugins
* **[nfdump](plugins/input/nfdump)** - NFDUMP file reader

###External storage plugins
* **[fastbit](plugins/input/fastbit)** - uses FastBit library to store and index data
* **[nfdump](plugins/storage/nfdump)** - stores data in NFDUMP file format
* **[postgres](plugins/storage/postgres)** - stores data into PostgreSQL database
* **[statistics](plugins/storage/statistics)** - uses RRD library to generate statistics for collected data
* **[unirec](plugins/storage/unirec)** - stores data in UniRec format

##Built-in tools
###ipfixviewer and ipfixconf
Destribed in IPFIXcol's [README](base/README.md)

##External tools

###fbitconvert
Converts data from NFDUMP file format into FastBit. Uses [IPFIXcol](base/README.md), [nfdump input plugin](plugins/input/nfdump) and [fastbit storage plugin](plugins/storage/fastbit).

More info in it's [README](tools/fbitconvert/README.md)

###fbitdump

Tool for manipulating IPFIX data in FastBit database format. It uses FastBit library to read and index data.

More info in it's [README](tools/fbitdump/README.md)

###fbitexpire

Daemon for removal old data.

More info in it's [README](tools/fbitexpire/README.md)

###fbitmerge

Tool for merging FastBit data (saves free disk space, reduces number of files..)

More info in it's [README](tools/fbitmerge/README.md)

##How to build
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

##RPM
Each part of framework supports building rpm packages by running

```sh
make rpm
```

##FastBit
Plugins and tools that uses FastBit file format need FasBit library installed. IPFIXcol framework uses it's own fork of FastBit library to keep compatibility.

IPFIXcol's FastBit library can be found [here](https://github.com/CESNET/libfastbit).