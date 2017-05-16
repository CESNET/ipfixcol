## fbitmerge

### Tool description

This tool merges FastBit data so they take up less disk space, have fewer files and working with them is faster.

### Examples

```sh
fbitmerge -b /dir/subdir/subdir-with-ic-prefixed-folders/ -k h -p ic
```

Move all subfolders from subdir-with-ic-prefixed-folders in format icYYYYMMDDHHmmSS into /dir/subdir and then merge them by hour
