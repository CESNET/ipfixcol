#include "Printer.h"
#include <iostream>

//static const char *msg_module = "json printer";

Printer::Printer(const pugi::xpath_node &config)
{
	(void) config;
}

void Printer::ProcessDataRecord(const std::string &record)
{
	std::cout << record;
}
