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

Server::Server()
    : pool_{nullptr}, port_{53}, sel_mode_{selectionMode::RANDOM}, rr_{0},
      resend_attempts_{2}, num_threads_{10},
      response_maxlength_{512}, debug_{false} {
  timeout_.tv_sec = 1;
  timeout_.tv_usec = 0;
}

Server::~Server() { delete pool_; }

bool Server::loadConfig(const char *filename) {
  FILE *fp;
  char line[256], buffer[256];
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

    if (strlen(begin) >= strlen("nameserver") &&
        !strncmp(begin, "nameserver", strlen("nameserver"))) {
      begin += strlen("nameserver");
      while (*begin != '\0' && isspace(*begin))
        begin++;
      if (!strncmp(begin, "default", strlen("default"))) {
        loadConfig("/etc/resolv.conf"); // recursion
        continue;
      }

      int i;
      for (i = 0;
           i < (sizeof(buffer) - 1) && *begin != '\0' && !isspace(*begin);
           buffer[i++] = *begin++)
        ;
      buffer[i] = '\0';

      struct in_addr addr;
      if (inet_pton(AF_INET, buffer, &addr) == 1) {
        dns_servers_.push_back(addr);
      } else {
        syslog(LOG_WARNING, "Invalid ip address at line %d\n", linecount);
      }
    } else if (strlen(begin) >= strlen("selection-mode") &&
               !strncmp(begin, "selection-mode", strlen("selection-mode"))) {
      begin += strlen("selection-mode");
      while (*begin != '\0' && isspace(*begin))
        begin++;
      if (!strncmp(begin, "random", strlen("random"))) {
        sel_mode_ = selectionMode::RANDOM;
      } else if (!strncmp(begin, "round-robin", strlen("round-robin"))) {
        sel_mode_ = selectionMode::ROUND_ROBIN;
      } else {
        syslog(LOG_WARNING,
               "Invalid selection-mode at line %d, defaulting to \"random\"\n",
               linecount);
        sel_mode_ = selectionMode::RANDOM;
      }
    } else if (strlen(begin) >= strlen("dns64-prefix") &&
               !strncmp(begin, "dns64-prefix", strlen("dns64-prefix"))) {
      begin += strlen("dns64-prefix");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      int i;
      for (i = 0; i < (sizeof(buffer) - 1) && *begin != '\0' && *begin != '/';
           buffer[i++] = *begin++)
        ;
      buffer[i] = '\0';
      if (*begin != '/' || sscanf(begin + 1, "%hhu", &ipv6_prefix_) != 1) {
        syslog(LOG_WARNING,
               "Invalid dns64-prefix at line %d: missing or bad prefix\n",
               linecount);
        success = false;
        break;
      }
      if (ipv6_prefix_ != 32 && ipv6_prefix_ != 40 && ipv6_prefix_ != 48 &&
          ipv6_prefix_ != 56 && ipv6_prefix_ != 64 && ipv6_prefix_ != 96) {
        syslog(LOG_WARNING,
               "Invalid dns64-prefix at line %d. Usable NDS64 prefix length "
               "values are: 32,40,48,56,64,96\n",
               linecount);
        success = false;
        break;
      }
      if (inet_pton(AF_INET6, buffer, &ipv6_) != 1) {
        syslog(LOG_WARNING, "Invalid dns64-prefix at line %d: bad address\n",
               linecount);
        success = false;
        break;
      }
    } else if (strlen(begin) >= strlen("debugging") &&
               !strncmp(begin, "debugging", strlen("debugging"))) {
      begin += strlen("debugging");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (!strncmp(begin, "yes", strlen("yes"))) {
        debug_ = true;
      } else {
        debug_ = false;
      }
    } else if (strlen(begin) >= strlen("timeout-time") &&
               !strncmp(begin, "timeout-time", strlen("timeout-time"))) {
      long int sec, usec;
      begin += strlen("timeout-time");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%ld.%ld", &sec, &usec) != 2 || sec < 0 ||
          sec > 32767 || usec < 0 || usec > 999999) {
        timeout_.tv_sec = 1;
        timeout_.tv_usec = 0;
        syslog(LOG_WARNING,
               "Invalid timeout-time at line %d. Defaulting to 1.0 sec\n",
               linecount);
        continue;
      } else {
        timeout_.tv_sec = sec;
        timeout_.tv_usec = usec;
      }
    } else if (strlen(begin) >= strlen("resend-attempts") &&
               !strncmp(begin, "resend-attempts", strlen("resend-attempts"))) {
      begin += strlen("resend-attempts");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%hd", &resend_attempts_) != 1 ||
          resend_attempts_ < 0) {
        resend_attempts_ = 2;
        syslog(LOG_WARNING,
               "Invalid resend-attempts at line %d. Defaulting to 2\n",
               linecount);
        continue;
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
    } else if (strlen(begin) >= strlen("response-maxlength") &&
               !strncmp(begin, "response-maxlength",
                        strlen("response-maxlength"))) {
      begin += strlen("response-maxlength");
      while (*begin != '\0' && isspace(*begin))
        begin++;

      if (sscanf(begin, "%hd", &response_maxlength_) != 1 ||
          response_maxlength_ < 0) {
        response_maxlength_ = 512;
        syslog(LOG_WARNING,
               "Invalid response-maxlength at line %d. Defaulting to 512\n",
               linecount);
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
  if (dns_servers_.size() == 0) {
    success = false;
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
  memset(&dns64srv_addr_, 0x00, sizeof(dns64srv_addr_));
  dns64srv_addr_.sin6_family = AF_INET6;   // Address family
  dns64srv_addr_.sin6_port = htons(port_); // UDP port number
  dns64srv_addr_.sin6_addr = in6addr_any;  // To any valid IP address
  if (bind(sock6fd_, reinterpret_cast<struct sockaddr *>(&dns64srv_addr_),
           sizeof(dns64srv_addr_)) == -1) {
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
  snprintf(buffer, sizeof(buffer), "DNS Servers:\n");
  os << buffer;
  for (auto &s : server.dns_servers_) {
    snprintf(buffer, sizeof(buffer), "%s\n", inet_ntoa(s));
    os << buffer;
  }
  snprintf(buffer, sizeof(buffer), "\n");
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Selection mode: ");
  os << buffer;
  if (server.sel_mode_ == Server::selectionMode::ROUND_ROBIN) {
    snprintf(buffer, sizeof(buffer), "round-robin\n");
    os << buffer;
  } else {
    snprintf(buffer, sizeof(buffer), "random\n");
    os << buffer;
  }
  char str[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &server.ipv6_, str, INET6_ADDRSTRLEN);
  snprintf(buffer, sizeof(buffer), "DNS64 IPv6 address: %s/%d\n", str,
           server.ipv6_prefix_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Debug mode: %s\n",
           server.debug_ ? "yes" : "no");
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Timeout: %ld.%ld\n", server.timeout_.tv_sec,
           server.timeout_.tv_usec);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Resend attempts: %hd\n",
           server.resend_attempts_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Maximum response length: %hd\n",
           server.response_maxlength_);
  os << buffer;
  snprintf(buffer, sizeof(buffer), "Worker threads: %hd\n",
           server.num_threads_);
  os << buffer;
  return os;
}

bool Server::debug() const { return debug_; }

void Server::synth(const uint8_t *v4, uint8_t *v6) {
  memset(v6, 0x00, 16);
  memcpy(v6, ipv6_.s6_addr, ipv6_prefix_ / 8);
  switch (ipv6_prefix_) {
  case 32:
    memcpy(v6 + 4, v4, 4);
    break;
  case 40:
    memcpy(v6 + 5, v4, 3);
    memcpy(v6 + 9, v4 + 3, 1);
    break;
  case 48:
    memcpy(v6 + 6, v4, 2);
    memcpy(v6 + 9, v4 + 2, 2);
    break;
  case 56:
    memcpy(v6 + 7, v4, 1);
    memcpy(v6 + 9, v4 + 1, 3);
    break;
  case 64:
    memcpy(v6 + 9, v4, 4);
    break;
  case 96:
    memcpy(v6 + 12, v4, 4);
    break;
  }
}
