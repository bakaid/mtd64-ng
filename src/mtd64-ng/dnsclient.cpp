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
 
#include "server.h"
#include "dnsclient.h"
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>

DNSClientException::DNSClientException(std::string what): what_{what} {}

const char* DNSClientException::what() const noexcept {
	return what_.c_str();
}

DNSClient::DNSClient(Server& dns_server): dns_server_{dns_server} {
	/* Create a UDP socket */
	if ((sockfd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		throw DNSClientException("Cannot create socket");
	}
	/* Set socket timeout to configured value */
	if (setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, (const void*) &dns_server_.timeout_, sizeof(dns_server_.timeout_))) {
		close(sockfd_);
		throw DNSClientException("Cannot set timeout: setsockopt failed");
	}
}

DNSClient::~DNSClient() {
	/* Close socket */
	close(sockfd_);
}

ssize_t DNSClient::sendQuery(uint8_t* query, size_t query_len, uint8_t* answer, size_t answer_len) {
	struct sockaddr_in server;
	socklen_t resp_len;
	ssize_t recvlen;
	short int attempts = 0;
	/* Attempt to get an answer, at most resend_attempts times. */
	while (attempts <= dns_server_.resend_attempts_) {
		memset(&server, 0x00, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(53);
		/* Use the configured selection mode to select the nameserver */
		if (dns_server_.sel_mode_ == Server::selectionMode::ROUND_ROBIN) {
			server.sin_addr = dns_server_.dns_servers_[(++dns_server_.rr_) % dns_server_.dns_servers_.size()];
		} else {
			server.sin_addr = dns_server_.dns_servers_[rand() % dns_server_.dns_servers_.size()];
		}
		/* Send DNS query */
		if (sendto(sockfd_, query, query_len, 0, (struct sockaddr*) &server, sizeof(server)) == -1) {
			throw DNSClientException("Cannot send query");
		}
		/* Receive DNS answer */
		if ((recvlen = recvfrom(sockfd_, answer, answer_len, 0, (struct sockaddr*) &server, &resp_len)) > 0) {
			return recvlen;
		}
		attempts++;
	}
	return -1;
}
