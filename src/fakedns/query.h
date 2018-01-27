/* fakeDNS - a fast authoritative DNS server for DNS64 testing purposes
 * Copyright (C) 2016  Daniel Bakai <bakaid@kszk.bme.hu>
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
 *  @brief Header for the Query and related classes.
 */

#ifndef QUERY_H_INCLUDED
#define QUERY_H_INCLUDED

#include "../dns.h"
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <syslog.h>
#include <thread>

class Server;

/**
 * Class to execute a DNS query.
 */
class Query {
private:
  uint8_t *data_;              /**< The packet */
  size_t len_;                 /**< The length of the packet. */
  int sock6fd_;                /**< Socket to send on. */
  struct sockaddr_in6 sender_; /**< The address of the sender of the packet. */
  socklen_t sender_slen_;      /**< The length of sender address. */
  Server &server_;             /**< The parent Server */
public:
  /**
   * Constructor.
   * @param data the packet
   * @param len the length of the packet
   * @param sender the address of the sender of the packet
   * @param sender_slen the length of sender address
   * @param server the parent Server
   */
  Query(uint8_t *data, size_t len, int sock6fd, struct sockaddr_in6 sender,
        socklen_t sender_slen, Server &server);

  /**
   * Copy constructor.
   * Needed because std::function, but preferably (and usually) optimized out by
   * the compiler.
   */
  Query(const Query &rhs);

  /**
   * Move constructor.
   */
  Query(Query &&rhs);

  /**
   * Copy assignment operator, explicitly deleted.
   */
  Query &operator=(const Query &) = delete;

  /**
   * Move assignment operator, explicitly deleted.
   */
  Query &operator=(Query &&) = delete;

  /**
   * Desctuctor.
   */
  ~Query();

  /**
   * Function call operator.
   * This makes the class a functor. Performs the main DNS64 action.
   */
  void operator()();
};
#endif
