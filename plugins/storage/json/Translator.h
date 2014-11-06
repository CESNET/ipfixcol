/* 
 * File:   Formatter.h
 * Author: michal
 *
 * Created on 14. říjen 2014, 15:27
 */

#ifndef TRANSLATOR_H
#define	TRANSLATOR_H

#include <string>
#include <vector>

enum class t_units {
    SEC,
    MILLISEC,
    MICROSEC,
    NANOSEC
};

class Translator {
public:
    /**
     * \brief Constructor
     */
    Translator();
    
    /**
     * \brief Destructor
     */
    ~Translator();
    
    /**
     * \brief Format IPv4 address into dotted format
     * 
     * @param addr address
     * @return formatted address
     */
    std::string formatIPv4(uint32_t addr);
    
    /**
     * \brief Format IPv6 address
     * 
     * @param addr address
     * @return formatted address
     */
    std::string formatIPv6(uint8_t *addr);
    
    /**
     * \brief Format MAC address
     * 
     * @param addr address
     * @return formatted address
     */
    std::string formatMac(uint8_t *addr);
    
    /**
     * \brief Format timestamp
     * 
     * @param tstamp timestamp
     * @param units time units
     * @return  formatted timestamp
     */
    std::string formatTimestamp(uint64_t tstamp, t_units units);
    
    /**
     * \brief Format protocol
     * 
     * @param proto protocol
     * @return formatted protocol
     */
    std::string formatProtocol(uint8_t proto);
    
    /**
     * \brief Format TCP flags
     * 
     * @param flags
     * @return formatted flags
     */
    std::string formatFlags(uint16_t flags);
    

private:
    
    std::vector<char> buffer{};
};

#endif	/* TRANSLATOR_H */

