/**
 * \file translator.c
 * \author Lukas Hutak <lukas.hutakcesnet.cz>
 * \brief Data type conversion functions
 */

/* Copyright (C) 2017 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

/*
 * NOTE: This file is a subset of IPFIX converters slightly modified for
 *       this plugin.
 */

#ifndef CONVERTERS_H
#define CONVERTERS_H

#include <libnf.h>
#include <stdint.h>    // uint8_t, etc.
#include <arpa/inet.h> // ntohs, etc.

/**
 * \def IPX_CONVERT_OK
 * \brief Status code for successful conversion
 */
#define IPX_CONVERT_OK          (0)

/**
 * \def IPX_CONVERT_ERR_ARG
 * \brief Status code for invalid argument(s) of a conversion function
 */
#define IPX_CONVERT_ERR_ARG     (-1)

/**
 * \def IPX_CONVERT_ERR_TRUNC
 * \brief Status code for a truncation of a value of an argument of a conversion
 *   function.
 */
#define IPX_CONVERT_ERR_TRUNC   (-2)

/**
 * \brief IPFIX Elements
 */
enum ipx_element_type {
	/**
	 * The type represents a time value expressed with second-level precision.
	 */
	IPX_ET_DATE_TIME_SECONDS,

	/**
	 * The type represents a time value expressed with millisecond-level
	 * precision.
	 */
	IPX_ET_DATE_TIME_MILLISECONDS,

	/**
	 * The type represents a time value expressed with microsecond-level
	 * precision.
	 */
	IPX_ET_DATE_TIME_MICROSECONDS,

	/**
	 * The type represents a time value expressed with nanosecond-level
	 * precision.
	 */
	IPX_ET_DATE_TIME_NANOSECONDS,
};

/**
 * \def IPX_CONVERT_EPOCHS_DIFF
 * \brief Time difference between NTP and UNIX epoch in seconds
 *
 * NTP epoch (1 January 1900, 00:00h) vs. UNIX epoch (1 January 1970 00:00h)
 * i.e. ((70 years * 365 days) + 17 leap-years) * 86400 seconds per day
 */
#define IPX_CONVERT_EPOCHS_DIFF (2208988800ULL)

/**
 * \brief Get a value of an unsigned integer (stored in big endian order a.k.a.
 *   network byte order)
 *
 * The \p value is read from a data \p field and converted from
 * the appropriate byte order to host byte order.
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[out] value  Pointer to a variable for the result
 * \return On success returns #IPX_CONVERT_OK and fills the \p value.
 *   Otherwise (usually the incorrect \p size of the field) returns
 *   #IPX_CONVERT_ERR_ARG and the \p value is not filled.
 */
static inline int
ipx_get_uint_be(const void *field, size_t size, uint64_t *value)
{
	switch (size) {
	case 8:
		*value = be64toh(*(const uint64_t *) field);
		return IPX_CONVERT_OK;

	case 4:
		*value = ntohl(*(const uint32_t *) field);
		return IPX_CONVERT_OK;

	case 2:
		*value = ntohs(*(const uint16_t *) field);
		return IPX_CONVERT_OK;

	case 1:
		*value = *(const uint8_t *) field;
		return IPX_CONVERT_OK;

	default:
		// Other sizes (3,5,6,7)
		break;
	}

	if (size == 0 || size > 8) {
		return IPX_CONVERT_ERR_ARG;
	}

	uint64_t new_value = 0;
	memcpy(&(((uint8_t *) &new_value)[8U - size]), field, size);

	*value = be64toh(new_value);
	return IPX_CONVERT_OK;
}

/**
 * \brief Set a value of an unsigned integer
 *
 * \param[out] field    Pointer to the data field
 * \param[in]  lnf_type Type of a LNF field
 * \param[in]  value    New value
 * \return On success returns #IPX_CONVERT_OK. If the \p value cannot fit
 *   in the \p field of the defined \p size, stores a saturated value and
 *   returns the value #IPX_CONVERT_ERR_TRUNC. If the \p lnf_type is not
 *   supported , a value of the \p field is unchanged and returns
 *   #IPX_CONVERT_ERR_ARG.
 */
static inline int
ipx_set_uint_lnf(void *field, int lnf_type, uint64_t value)
{
	switch (lnf_type) {
	case LNF_UINT64:
		*((uint64_t *) field) = value;
		return IPX_CONVERT_OK;

	case LNF_UINT32:
		if (value > UINT32_MAX) {
			*((uint32_t *) field) = UINT32_MAX; // byte conversion not required
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((uint32_t *) field) = (uint32_t) value;
		return IPX_CONVERT_OK;

	case LNF_UINT16:
		if (value > UINT16_MAX) {
			*((uint16_t *) field) = UINT16_MAX; // byte conversion not required
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((uint16_t *) field) = (uint16_t) value;
		return IPX_CONVERT_OK;

	case LNF_UINT8:
		if (value > UINT8_MAX) {
			*((uint8_t *) field) = UINT8_MAX;
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((uint8_t *) field) = (uint8_t) value;
		return IPX_CONVERT_OK;

	default:
		return IPX_CONVERT_ERR_ARG;
	}
}

/**
 * \brief Get a value of a low precision timestamp (stored in big endian order
 *   a.k.a. network byte order)
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to host byte order and transformed to a corresponding
 * data type.
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (in bytes)
 * \param[in]  type   Type of the timestamp (see the remark)
 * \param[out] value  Pointer to a variable for the result (Number of
 *   milliseconds since the UNIX epoch)
 * \remark The parameter \p type can be only one of the following types:
 *   #IPX_ET_DATE_TIME_SECONDS, #IPX_ET_DATE_TIME_MILLISECONDS,
 *   #IPX_ET_DATE_TIME_MICROSECONDS, #IPX_ET_DATE_TIME_NANOSECONDS
 * \warning The \p size of the \p field MUST be 4 bytes
 *   (#IPX_ET_DATE_TIME_SECONDS) or 8 bytes (#IPX_ET_DATE_TIME_MILLISECONDS,
 *   #IPX_ET_DATE_TIME_MICROSECONDS, #IPX_ET_DATE_TIME_NANOSECONDS)
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \return On success returns #IPX_CONVERT_OK and fills the \p value.
 *   Otherwise (usually the incorrect \p size of the field) returns
 *   #IPX_CONVERT_ERR_ARG and the \p value is not filled.
 */
static inline int
ipx_get_datetime_lp_be(const void *field, size_t size, enum ipx_element_type type,
		uint64_t *value)
{
	// One second to milliseconds
	const uint64_t S1E3 = 1000ULL;

	if ((size != sizeof(uint64_t) || type == IPX_ET_DATE_TIME_SECONDS)
			&& (size != sizeof(uint32_t) || type != IPX_ET_DATE_TIME_SECONDS)) {
		return IPX_CONVERT_ERR_ARG;
	}

	switch (type) {
	case IPX_ET_DATE_TIME_SECONDS:
		*value = ntohl(*(const uint32_t *) field) * S1E3;
		return IPX_CONVERT_OK;

	case IPX_ET_DATE_TIME_MILLISECONDS:
		*value = be64toh(*(const uint64_t *) field);
		return IPX_CONVERT_OK;

	case IPX_ET_DATE_TIME_MICROSECONDS:
	case IPX_ET_DATE_TIME_NANOSECONDS: {
		// Conversion from NTP 64bit timestamp to UNIX timestamp
		const uint32_t (*parts)[2] = (const uint32_t (*)[2]) field;
		uint64_t result;

		// Seconds
		result = (ntohl((*parts)[0]) - IPX_CONVERT_EPOCHS_DIFF) * S1E3;

		/*
		 * Fraction of second (1 / 2^32)
		 * The "value" uses 1/1000 sec as unit of subsecond fractions and NTP
		 * uses 1/(2^32) sec as its unit of fractional time.
		 * Conversion is easy: First, multiply by 1e3, then divide by 2^32.
		 * Warning: Calculation must use 64bit variable!!!
		 */
		uint64_t fraction = ntohl((*parts)[1]);
		if (type == IPX_ET_DATE_TIME_MICROSECONDS) {
			fraction &= 0xFFFFF800UL; // Make sure that last 11 bits are zeros
		}

		result += (fraction * S1E3) >> 32;
		*value = result;
	}

		return IPX_CONVERT_OK;
	default:
		return IPX_CONVERT_ERR_ARG;
	}
}

#endif // CONVERTERS_H
