**Future release:**

**Version 1.5.0:**

* Minor tweaks in -part.txt
* Fixed prefix error if name_type == "prefix"
* Various coding style improvements
* Updated pugixml from v1.0 to v1.6
* Improved resilience against malfored variable-length specs
* Consistently using unsigned integers to avoid casting to wider (signed) int
* Removed duplicate logging
* Added support for old gnu++0x standard, forcing gnu++11 where necessary
* Data types for enterprise and field IDs are now declared more precisely
* Fixed dir_check function return value. The issue caused wrong writeout of buffered data
* Added/fixed Arch Linux support
* Fixed uninitialized variable `last_flush`
