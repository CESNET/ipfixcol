#ifndef PRINTER_H
#define PRINTER_H

#include "nfdump_ext.h"

class Printer : public Output
{
public:
	Printer(const pugi::xpath_node& config);

	void ProcessDataRecord(const std::string& record);
};

#endif // PRINTER_H
