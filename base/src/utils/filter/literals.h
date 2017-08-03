//
// Created by istoffa on 7/28/17.
//

#include <inttypes.h>

#ifndef IPFIXCOL_RVALUES_H
#define IPFIXCOL_RVALUES_H

typedef struct nff_literal_s {
    const char* name;
    const uint64_t value;
}nff_literal_t;


struct nff_literal_s * nff_get_protocol_map();
struct nff_literal_s * nff_get_port_map();

#endif //IPFIXCOL_RVALUES_H
