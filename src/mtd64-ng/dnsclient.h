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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

/** @file
 *  @brief Header for the DNSClient and related classes.
 */

#ifndef DNSCLIENT_H_INCLUDED
#define DNSCLIENT_H_INCLUDED
#include "dnssource.h"
#include <exception>
#include <netinet/in.h>
#include <stdint.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

class Server;

/**
 * An std::exception class for the DNSClient.
 */
class DNSClientException : public std::exception {
private:
  std::string what_; /**< Exception string */
public:
  /**
   * A constructor.
   * @param what the excpetion string
   */
  DNSClientException(std::string what);

  /**
   * A getter for the exception string.
   * @return the exception string
   */
  const char *what() const noexcept override;
};

/**
 * A DNSClient implementation using the configured recursors and no caching.
 */
class DNSClient : public DNSSource {
private:
  Server
      &dns_server_; /**< The Server, used to access configuration settings. */
  int sockfd_;      /**< Socket for sending the DNS query. */
public:
  /**
   * Constructor.
   * @return the Server to use
   */
  DNSClient(Server &dns_server);

  /**
   * Destructor.
   */
  ~DNSClient();

  /**
   * Uses a configured DNS server (and the configured selection mode) to get an
   * answer.
   * @param query the query packet
   * @param query_len length of the packet
   * @param answer buffer for the answer
   * @param answer_len length of the buffer
   * @return the length of the answer (-1 on failure)
   */
  ssize_t sendQuery(uint8_t *query, size_t query_len, uint8_t *answer,
                    size_t answer_len) override;
};

#endif
