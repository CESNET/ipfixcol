## fbitdump

### Tool description

fbitdump is a tool for manipulating IPFIX data in FastBit database format. It uses FastBit library to read and index data.
There are many command line options - see man pages for their description.

### Configuration

fbitdump uses **fbitdump.xml** file with configured data columns, output formats etc.

### Output formats

There are few predefined formats specified in **fbitdump.xml**. Each format is defined as string containing column aliases starting with %.
Custom formats are supported.

### Filter

The filter can be either specified on the command line after all options or in a separate file. It can span several lines.
Specifying an element in the filter makes fbitdump to select only the tables with such an element.

Any filter consists of one or more expression expr. Any number of expr can be linked together with and or or keyword. This is case independent.

### Plugins

Fbitdump supports input and output plugins. Input plugins are used for parsing data from given filter and output plugins are used for valid formatting printed data.
Plugin names and paths must be specified in **fbitdump.xml** file.

For output format description, see man pages for fbitdump-plugins.

Fbitdump already contains one default plugin for parsing and formatting data for columns without specified plugin.

### Examples

```sh 
fbitdump -R /dir/dir-with-templates/template -o line4 "%dstport = 80" -c 100
```
Read one template folder, corresponds to -r nfcapd.xxxx in nfdump and filtering for specific template, i.e. ICMP. Outputs first 100 flows with destination port 80 using line4 format.

```sh
fbitdump -R /dir/ -A%dstip4,%srcport "%dstip4 = 192.168.1.1"
```
Reads all subdirs in dir recursively. Outputs flows with destination IPv4 192.168.1.1 aggregated by source port (and IPv4 destination address) using default format

