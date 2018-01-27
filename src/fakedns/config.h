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
 *  @brief Header for the Config and related classes.
 */

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <atomic>
#include <exception>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/time.h>
#include <thread>
#include <vector>

/**
 * An std::exception class for the ServerConfig.
 */
class ConfigException : public std::exception {
private:
  std::string what_; /**< Exception string */
public:
  /**
   * A constructor.
   * @param what the excpetion string
   */
  ConfigException(std::string what);

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
struct Config {
  /**
   * Enum for the server AAAA behaviour
   */
  enum aaaaMode {
    YES,        /**< always has AAAA record */
    NO,         /**< never has AAAA record */
    PROBABILITY /**< has AAAA record with some PROBABILITY  */
  };

  uint16_t start_port_; /**< First server processes port. */

  static const unsigned short int response_maxlength_ =
      512; /**< Maximum legth of the DNS response packet (UDP payload) */

  aaaaMode aaaa_mode_; /**< AAAA behaviour */

  double aaaa_probability_; /**< AAAA probability when using PROBABILITY
                               behaviour */

  short int num_servers_; /**< Number of server processes to start */

  long int start_cpu_;

  bool debug_; /**< Debug flag */

  /**
   * Constructor.
   */
  Config();

  /**
   * Function to load configuration file
   * @param filename full path to configuration file
   * @return whether the loading was successful
   */
  bool loadConfig(const char *filename);

  /**
   * std::ostream operator to display configuration values
   */
  friend std::ostream &operator<<(std::ostream &, const Config &);
};

std::ostream &operator<<(std::ostream &, const Config &);

#endif
