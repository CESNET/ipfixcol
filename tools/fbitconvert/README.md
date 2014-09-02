##fbitconvert

###Tool description

This tool converts nfcapd file(s) into FastBit database.  It combines [IPFIXcol](../../base/), [nfdump input plugin](../../plugins/input/nfdump/) and [FastBit storage plugin](../../plugins/storage/fastbit/).

All of possible options (except specifying nfcapd source file) are used to configure storage plugin so look at it's [README](../../plugins/storage/fastbit/) for more informations.


###Example

```sh
fbitconvert --source=/path/to/nfcapd.file
```
