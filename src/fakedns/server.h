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
 *  @brief Header for the Server and related classes.
 */

#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include <atomic>
#include <exception>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/time.h>
#include <thread>
#include <vector>

#include "config.h"

/**
 * An std::exception class for the Server.
 */
class ServerException : public std::exception {
private:
  std::string what_; /**< Exception string */
public:
  /**
   * A constructor.
   * @param what the excpetion string
   */
  ServerException(std::string what);

  /**
   * A getter for the exception string.
   * @return the exception string
   */
  const char *what() const noexcept override;
};

/**
 * Main Server class.
 * This class aggregates the server parameters and functions.
 */
class Server {
public:
  /**
   * Constructor.
   */
  Server(const Config &config);

  /**
   * Destructor.
   */
  ~Server();

  /**
   * Copy constructor, explicitly deleted.
   * The Server class can NOT be copied.
   */
  Server(const Server &) = delete;

  /**
   * Move constructor, explicitly deleted.
   * The Server class can NOT be moved.
   */
  Server(Server &&) = delete;

  /**
   * Copy assignment operator, explicitly deleted.
   * The Server class can NOT be copied.
   */
  Server &operator=(const Server &) = delete;

  /**
   * Move assignment operator, explicitly deleted.
   * The Server class can NOT be moved.
   */
  Server &operator=(Server &&) = delete;

  /**
   * Function to load configuration file
   * @param filename full path to configuration file
   * @return whether the loading was successful
   */
  bool loadConfig(const char *filename);

  /**
   * Function to start the server
   */
  void start();

  /**
   * Function to start the server
   */
  void stop();

  /**
   * std::ostream operator to display configuration values
   */
  friend std::ostream &operator<<(std::ostream &, const Server &);

  /**
   * Query uses and can modify the Server (thread-safely).
   */
  friend class Query;

private:
  std::atomic<bool>
      stopped_; /**< Atomic variable used to thread-safely stop the pool. */

  Config config_; /**< Server configuration. */

  struct in6_addr ipv6_; /**< Prefix used for generating AAAA records */

  /**
   * Generate AAAA record
   * @param v4 the IPv4 address in network byte order (4 bytes)
   * @param v6 the buffer to store the IPv6 address (at least 16 bytes)
   */
  void synth(const uint8_t *v4, uint8_t *v6);
};

std::ostream &operator<<(std::ostream &, const Server &);

#endif
