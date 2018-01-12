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

#include "server.h"
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

#include "query.h"

ServerException::ServerException(std::string what) : what_{what} {}

const char *ServerException::what() const noexcept { return what_.c_str(); }

Server::Server() : pool_{nullptr}, port_{53}, num_threads_{10}, debug_{false} {
  inet_pton(AF_INET6, "2001:db8::", &ipv6_);
}

Server::~Server() { delete pool_; }

bool Server::loadConfig(const char *filename) {
  FILE *fp;
  char line[256];
  char *begin;
  int linecount;
  bool success;

  if ((fp = fopen(filename, "r")) == NULL) {
    throw ServerException("Missing configuration file!");
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
    } else if (strlen(begin) >= strlen("num-threads") &&
               !strncmp(begin, "num-threads", strlen("num-threads"))) {
      begin += strlen("num-threads");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%hd", &num_threads_) != 1 || num_threads_ < 0) {
        num_threads_ = 10;
        syslog(LOG_WARNING,
               "Invalid num-threads at line %d. Defaulting to 10\n", linecount);
        continue;
      }
    } else if (strlen(begin) >= strlen("port") &&
               !strncmp(begin, "port", strlen("port"))) {
      begin += strlen("port");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%hu", &port_) != 1) {
        port_ = 53;
        syslog(LOG_WARNING, "Invalid port at line %d. Defaulting to 53\n",
               linecount);
        continue;
      }
    }
  }
  fclose(fp);
  return success;
}

void Server::start() {
  /* Creating socket */
  if ((sock6fd_ = socket(AF_INET6, SOCK_DGRAM, 0)) == -1) {
    throw ServerException{"Unable to create server socket"};
  }

  /* Binding socket */
  memset(&fakednssrv_addr_, 0x00, sizeof(fakednssrv_addr_));
  fakednssrv_addr_.sin6_family = AF_INET6;   // Address family
  fakednssrv_addr_.sin6_port = htons(port_); // UDP port number
  fakednssrv_addr_.sin6_addr = in6addr_any;  // To any valid IP address
  if (bind(sock6fd_, reinterpret_cast<struct sockaddr *>(&fakednssrv_addr_),
           sizeof(fakednssrv_addr_)) == -1) {
    std::stringstream ss;
    ss << "Unable to bind server socket: " << strerror(errno);
    throw ServerException{ss.str()};
  }

  /* Creating worker pool */
  pool_ = new ThreadPool{static_cast<size_t>(num_threads_)};

  /* Receving packets */
  while (!pool_->isStopped()) {
    struct sockaddr_in6 sender;
    socklen_t sender_slen;
    ssize_t recvlen;
    char client_ip[INET6_ADDRSTRLEN];
    uint8_t *buffer = new uint8_t[response_maxlength_];
    sender_slen = sizeof(sender);
    if ((recvlen = recvfrom(sock6fd_, buffer, response_maxlength_, 0,
                            reinterpret_cast<struct sockaddr *>(&sender),
                            &sender_slen)) <= 0) {
      delete[] buffer;
      if (errno == EMSGSIZE) {
        syslog(LOG_DAEMON | LOG_WARNING,
               "The received message from IPv6 client is longer than %hd "
               "bytes. Ignored",
               response_maxlength_);
        continue;
      } else if (errno == EINTR) {
        break;
      } else {
        syslog(LOG_DAEMON | LOG_WARNING, "recvfrom() failure: %d (%s)", errno,
               strerror(errno));
        continue;
      }
    }
    inet_ntop(AF_INET6, &sender.sin6_addr, client_ip, INET6_ADDRSTRLEN);
    syslog(LOG_DAEMON | LOG_INFO, "Received packet from [%s]:%hu, length %zd",
           client_ip, ntohs(sender.sin6_port), recvlen);

    pool_->addTask(Query{buffer, (size_t)recvlen, sender, sender_slen, *this});
  }
  close(sock6fd_);
}

void Server::stop() { pool_->stop(); }

std::ostream &operator<<(std::ostream &os, const Server &server) {
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "AAAA mode: ");
  os << buffer;
  if (server.aaaa_mode_ == Server::aaaaMode::YES) {
    snprintf(buffer, sizeof(buffer), "1\n");
    os << buffer;
  } else if (server.aaaa_mode_ == Server::aaaaMode::NO) {
    snprintf(buffer, sizeof(buffer), "0\n");
    os << buffer;
  } else {
    snprintf(buffer, sizeof(buffer), "%f\n", server.aaaa_probability_);
    os << buffer;
  }
  snprintf(buffer, sizeof(buffer), "Worker threads: %hd\n",
           server.num_threads_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Port: %hu\n", server.port_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Debug mode: %s\n",
           server.debug_ ? "yes" : "no");
  os << buffer;
  return os;
}

bool Server::debug() const { return debug_; }

void Server::synth(const uint8_t *v4, uint8_t *v6) {
  memcpy(v6, ipv6_.s6_addr, sizeof(ipv6_.s6_addr));
  for (int i = 0; i < 4; i++) {
    v6[12 + i] = v4[i];
  }
}
