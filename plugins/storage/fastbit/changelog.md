**Future release:**

**Version 1.6.0:**

* Replaced use of get_type_from_xml by get_element_by_id
* Fixed build with latest Fastbit library
* Code consistency improvements, lots of refactoring
* Fixed handling of large PENs
* Fixed extern "C" problems reported by J Thomas
* Fixed uninitialized variable
* Unknown elements with variable size are now stored as blobs
* Creation of .sp files is configurable from startup config
* Added handling for multiple elements with same name. Storing only the first one in the template.

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
