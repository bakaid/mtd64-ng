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
 *  @brief Header for the Server and related classes.
 */

#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include "../pool.h"
#include <atomic>
#include <exception>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/time.h>
#include <vector>

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
   * Enum for the server selection mode.
   */
  enum selectionMode {
    ROUND_ROBIN, /**< round-robin selection */
    RANDOM       /**< random selection */
  };

  /**
   * Constructor.
   */
  Server();

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
   * Getter for the debug_ variable.
   * @return whether the Server is configured to use debug mode
   */
  bool debug() const;

  /**
   * std::ostream operator to display configuration values
   */
  friend std::ostream &operator<<(std::ostream &, const Server &);

  /**
   * Query uses and can modify the Server (thread-safely).
   */
  friend class Query;

  /**
   * DNSClient uses and can modify the Server (thread-safely).
   */
  friend class DNSClient;

private:
  ThreadPool *pool_; /**< ThreadPool to process queries on multiple threads. */

  std::vector<struct in_addr> dns_servers_; /**< Configured recursors to use. */

  int sock6fd_;                       /**< Server socket. */
  struct sockaddr_in6 dns64srv_addr_; /**< Server address. */
  uint16_t port_;                     /**< Server port. */

  selectionMode sel_mode_; /**< DNS server selection mode: (1) means
                              round-robin, (2) means random */
  std::atomic<int> rr_;    /**< The sequence number of the DNS server which is
                              actually in use in round-robin mode */

  struct in6_addr ipv6_; /**< For checking if the address is valid, and used
                            later for conversion, too */
  unsigned char
      ipv6_prefix_; /**< Prefix length for IPv4 embedded IPv6 addresses */

  struct timeval timeout_; /**< DNS response packet arrival expectation time */
  short int resend_attempts_; /**< 0 = no resending attempt */

  short int num_threads_; /**< Number of worker threads to use */

  short int response_maxlength_; /**< Maximum legth of the IPv6 DNS response
                                    packet (UDP payload) */

  bool debug_; /**< Debug flag */

  /**
   * Function to synthesize the IPv6 address.
   * As described in RFC 6052 2.
   * @param v4 the IPv4 address in network byte order (4 bytes)
   * @param v6 the buffer to store the synthesized IPv6 address (at least 16
   * bytes)
   */
  void synth(const uint8_t *v4, uint8_t *v6);
};

std::ostream &operator<<(std::ostream &, const Server &);

#endif
