/**
 * \file fbitexpire.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Main body of the fbitexpire
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
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


#ifndef FBITEXPIRE_H_
#define FBITEXPIRE_H_

#include <iostream>
#include <thread>

/* Macros for size conversion */
#define KILOBYTE (1024)
#define MEGABYTE (KILOBYTE * 1024)
#define GIGABYTE (MEGABYTE * 1024)
#define TERABYTE (GIGABYTE * 1024)

#define BYTES_TO_KB(_size_) (_size_) / 1024.0
#define BYTES_TO_MB(_size_) (BYTES_TO_KB((_size_))) / 1024.0
#define BYTES_TO_GB(_size_) (BYTES_TO_MB((_size_))) / 1024.0
#define BYTES_TO_TB(_size_) (BYTES_TO_GB((_size_))) / 1024.0

#define KB_TO_BYTES(_size_) (_size_) * KILOBYTE
#define MB_TO_BYTES(_size_) (_size_) * MEGABYTE
#define GB_TO_BYTES(_size_) (_size_) * GIGABYTE
#define TB_TO_BYTES(_size_) (_size_) * TERABYTE


namespace fbitexpire {

/**
 * \brief Base class for Watcher, Scanner and Cleaner
 */
class FbitexpireThread {
public:
    /**
     * \brief Run thread
     */
    virtual void run()          { _done = false; _th = std::move(std::thread(&FbitexpireThread::loop, this)); }
    
    /**
     * \brief Stop and join thread
     */
    virtual void stop()         { _done = true; if (_th.joinable()) { _th.join(); } } 
    
    /**
     * \brief Destructor - stop and join thread
     */
    virtual ~FbitexpireThread() { stop(); }
    
    /**
     * \brief Is job done?
     * \return true if thread is over
     */
    bool     isDone()           { return _done; }
protected:
    /**
     * \brief Main function called from run() in separate thread
     */
    virtual void loop()         = 0;
    
    bool _done{true};  /**< done flag */
    std::thread _th;   /**< thread variable */
};

} /* end of namespace fbitexpire */

#endif /* FBITEXPIRE_H_ */
