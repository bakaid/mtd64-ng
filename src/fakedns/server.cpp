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

Server::Server() : stopped_{false}, port_{53}, num_threads_{10}, debug_{false} {
  inet_pton(AF_INET6, "2001:db8::", &ipv6_);
}

Server::~Server() {}

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
  for (int i = 0; i < this->num_threads_; i++) {
    threads_.emplace_back([this]() {
      /* Creating socket */
      int sock6fd;
      if ((sock6fd = socket(AF_INET6, SOCK_DGRAM, 0)) == -1) {
        throw ServerException{"Unable to create server socket"};
      }

      /* Setting socket options */
      int optval = 1;
      if (setsockopt(sock6fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                     sizeof(optval)) != 0) {
        throw ServerException{
            "Cannot set SO_REUSEADDR on socket: setsockopt failed"};
      }

      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      if (setsockopt(sock6fd, SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const void *>(&timeout),
                     sizeof(timeout))) {
        throw ServerException{"Cannot set timeout: setsockopt failed"};
      }

      /* Binding socket */
      struct sockaddr_in6 fakednssrv_addr;
      memset(&fakednssrv_addr, 0x00, sizeof(fakednssrv_addr));
      fakednssrv_addr.sin6_family = AF_INET6;         // Address family
      fakednssrv_addr.sin6_port = htons(this->port_); // UDP port number
      fakednssrv_addr.sin6_addr = in6addr_any;        // To any valid IP address
      if (bind(sock6fd, reinterpret_cast<struct sockaddr *>(&fakednssrv_addr),
               sizeof(fakednssrv_addr)) == -1) {
        std::stringstream ss;
        ss << "Unable to bind server socket: " << strerror(errno);
        throw ServerException{ss.str()};
      }

      /* Receving packets */
      std::vector<uint8_t> buffer(this->response_maxlength_);

      while (!this->stopped_) {
        struct sockaddr_in6 sender;
        socklen_t sender_slen;
        ssize_t recvlen;
        char client_ip[INET6_ADDRSTRLEN];
        sender_slen = sizeof(sender);
        if ((recvlen = recvfrom(sock6fd, buffer.data(), response_maxlength_, 0,
                                reinterpret_cast<struct sockaddr *>(&sender),
                                &sender_slen)) <= 0) {
          if (errno == EMSGSIZE) {
            syslog(LOG_DAEMON | LOG_WARNING,
                   "The received message from IPv6 client is longer than %hd "
                   "bytes. Ignored",
                   response_maxlength_);
            continue;
          } else if (errno == EINTR) {
            break;
          } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
            continue;
          } else {
            syslog(LOG_DAEMON | LOG_WARNING, "recvfrom() failure: %d (%s)",
                   errno, strerror(errno));
            continue;
          }
        }
        inet_ntop(AF_INET6, &sender.sin6_addr, client_ip, INET6_ADDRSTRLEN);
        syslog(LOG_DAEMON | LOG_INFO,
               "Received packet from [%s]:%hu, length %zd", client_ip,
               ntohs(sender.sin6_port), recvlen);

        Query query{buffer.data(), static_cast<size_t>(recvlen),
                    sock6fd,       sender,
                    sender_slen,   *this};
        query();
      }

      close(sock6fd);
    });
  }

  for (auto &thread : this->threads_) {
    thread.join();
  }
}

void Server::stop() { this->stopped_ = true; }

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
