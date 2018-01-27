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

#include "config.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <net/if.h>
#include <netinet/in.h>
#include <sstream>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

ConfigException::ConfigException(std::string what) : what_{what} {}

const char *ConfigException::what() const noexcept { return what_.c_str(); }

Config::Config()
    : start_port_{1053}, num_servers_{8}, start_cpu_{1}, debug_{false} {}

bool Config::loadConfig(const char *filename) {
  FILE *fp;
  char line[256];
  char *begin;
  int linecount;
  bool success;

  if ((fp = fopen(filename, "r")) == NULL) {
    throw ConfigException("Missing configuration file!");
  }
  linecount = 0;
  success = true;
  while (fgets(line, sizeof(line), fp) != NULL) {
    linecount++;
    if (strlen(line) < 3 || line[0] == '#' ||
        (line[0] == '/' && line[1] == '/'))
      continue; // Skip comments
    if (strlen(line) == (sizeof(line) - 1) && line[sizeof(line) - 2] != '\n') {
      int c;
      while ((c = fgetc(fp)) != EOF) {
        if (c == '\n')
          break;
      }
    } // If a line is longer than the max line length, the rest of it is skipped

    begin = line;
    while (*begin != '\0' && isspace(*begin))
      begin++; // Skip leading whitespace

    if (strlen(begin) >= strlen("have-AAAA") &&
        !strncmp(begin, "have-AAAA", strlen("have-AAAA"))) {
      begin += strlen("have-AAAA");
      while (*begin != '\0' && isspace(*begin))
        begin++;
      if (*begin == '1') {
        aaaa_mode_ = YES;
      } else if (*begin == '0' && *(begin + 1) != '.') {
        aaaa_mode_ = NO;
      } else {
        aaaa_mode_ = PROBABILITY;
        if (sscanf(begin, "%lf", &aaaa_probability_) != 1 ||
            (aaaa_probability_ < 0.0 || aaaa_probability_ > 1.0)) {
          aaaa_mode_ = NO;
          syslog(LOG_WARNING, "Invalid hava-AAAA at line %d. Defaulting to 0\n",
                 linecount);
          continue;
        }
      }
    } else if (strlen(begin) >= strlen("debug") &&
               !strncmp(begin, "debug", strlen("debug"))) {
      begin += strlen("debug");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (!strncmp(begin, "yes", strlen("yes"))) {
        debug_ = true;
      } else {
        debug_ = false;
      }
    } else if (strlen(begin) >= strlen("num-servers") &&
               !strncmp(begin, "num-servers", strlen("num-servers"))) {
      begin += strlen("num-servers");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%hd", &num_servers_) != 1 || num_servers_ < 0) {
        num_servers_ = 10;
        syslog(LOG_WARNING, "Invalid num-servers at line %d. Defaulting to 8\n",
               linecount);
        continue;
      }
    } else if (strlen(begin) >= strlen("start-cpu") &&
               !strncmp(begin, "start-cpu", strlen("start-cpu"))) {
      begin += strlen("start-cpu");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%ld", &start_cpu_) != 1 || start_cpu_ < 0) {
        start_cpu_ = 1;
        syslog(LOG_WARNING, "Invalid start-cpu at line %d. Defaulting to 1\n",
               linecount);
        continue;
      }
    } else if (strlen(begin) >= strlen("start-port") &&
               !strncmp(begin, "start-port", strlen("start-port"))) {
      begin += strlen("start-port");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%hu", &start_port_) != 1) {
        start_port_ = 1053;
        syslog(LOG_WARNING,
               "Invalid start-port at line %d. Defaulting to 1053\n",
               linecount);
        continue;
      }
    }
  }
  fclose(fp);
  return success;
}

std::ostream &operator<<(std::ostream &os, const Config &config) {
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "AAAA mode: ");
  os << buffer;
  if (config.aaaa_mode_ == Config::aaaaMode::YES) {
    snprintf(buffer, sizeof(buffer), "1\n");
    os << buffer;
  } else if (config.aaaa_mode_ == Config::aaaaMode::NO) {
    snprintf(buffer, sizeof(buffer), "0\n");
    os << buffer;
  } else {
    snprintf(buffer, sizeof(buffer), "%f\n", config.aaaa_probability_);
    os << buffer;
  }
  snprintf(buffer, sizeof(buffer), "Server processes: %hd\n",
           config.num_servers_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Starting cpu: %ld\n", config.start_cpu_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Start port: %hu\n", config.start_port_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Debug mode: %s\n",
           config.debug_ ? "yes" : "no");
  os << buffer;
  return os;
}
