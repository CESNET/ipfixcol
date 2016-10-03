#ifndef SENDER_H
#define SENDER_H

#include "json.h"

extern "C" {
#include <siso.h>
}

class Sender : public Output
{
public:
	Sender(const pugi::xpath_node &config);

	~Sender();
	void ProcessDataRecord(const std::string& record);

private:
	sisoconf *sender{NULL};
	struct timeval connection_time;
};

#endif // SENDER_H
