/* mtd64-ng - a lightweight multithreaded C++11 DNS64 server
 * Based on MTD64 (https://github.com/Yoso89/MTD64)
 * Copyright (C) 2015  Daniel Bakai <bakaid@kszk.bme.hu>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
 
/** @file 
 *  @brief Header for the DNSSource interface.
 */
 
#ifndef DNSSOURCE_H_INCLUDED
#define DNSSOURCE_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

/**
 * DNSSource interface.
 * Classes implementing this interface can use their own caching strategies.
 */
class DNSSource {
	public:
		/**
		 * Sends a DNS query, writes the response into the answer buffer and returns with the answer length.
		 * @param query the query packet
		 * @param query_len length of the packet
		 * @param answer buffer for the answer
		 * @param answer_len length of the buffer
		 * @return the length of the answer (-1 on failure)
		 */
		virtual ssize_t sendQuery(uint8_t* query, size_t query_len, uint8_t* answer, size_t answer_len) = 0;
		
		/**
		 * Virtual desctrutor.
		 * Empty. Allows the classes implementing the interace to have own destructors.
		 */
		virtual ~DNSSource() {}
};

#endif
