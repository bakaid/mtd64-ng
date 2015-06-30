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
#include "query.h"
#include "dnsclient.h"
#include <algorithm>
#include <stdint.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

Query::Query(uint8_t* data, size_t len, struct sockaddr_in6 sender, socklen_t sender_slen, Server& server): data_{data},
																											 len_{len},
																											 sender_slen_{sender_slen},
																											 server_{server}
{
	sender_ = sender;
}

Query::Query(const Query& rhs): data_{rhs.data_},
								len_{rhs.len_},
								sender_slen_{rhs.sender_slen_},
								server_{rhs.server_}
{
	sender_ = rhs.sender_;
}

Query::Query(Query&& rhs): data_{rhs.data_},
						   len_{rhs.len_},
						   sender_slen_{rhs.sender_slen_},
						   server_{rhs.server_}
{
	sender_ = rhs.sender_;
	rhs.data_ = nullptr;
}

Query::~Query() {
	delete[] data_;
}
		
void Query::operator()() {
	ssize_t res;
	DNSHeader* header = (DNSHeader*) data_;
	if (header->qr() == 0 && header->opcode() == DNSHeader::OpCode::Query) {
		try {
			std::unique_ptr<uint8_t> answer{new uint8_t[server_.response_maxlength_]};
			std::unique_ptr<DNSSource> s{new DNSClient{server_}};
			if ((res = s->sendQuery(data_, len_, answer.get(), server_.response_maxlength_)) <= 0) {
				syslog(LOG_DAEMON | LOG_INFO, "Didn't receive answer from the nameservers");
				return;
			}
			DNSPacket packet{answer.get(), (size_t) res, (size_t) server_.response_maxlength_};
			if (packet.question_[0].qtype() == QType::AAAA &&
			    (packet.header_->rcode() != DNSHeader::RCODE::NXDomain &&
			    !(packet.header_->rcode() != DNSHeader::RCODE::NXDomain && packet.header_->ancount() >= 1 &&
			      std::find_if(packet.answer_.begin(), packet.answer_.end(), [](const DNSResource& r) {
					  return r.qtype() == QType::AAAA;
				  }) != packet.answer_.end())
			   )) {
					// Synthesizing
					DNSPacket qpacket{data_, len_, len_};
					qpacket.question_[0].qtype(QType::A);
					if ((res = s->sendQuery(data_, len_, answer.get(), server_.response_maxlength_)) <= 0) {
						syslog(LOG_DAEMON | LOG_INFO, "Didn't receive answer from the nameservers");
						return;
					}
					DNSPacket apacket{answer.get(), (size_t) res, (size_t) server_.response_maxlength_};
					apacket.question_[0].qtype(QType::AAAA);
					for (auto& answer: apacket.answer_) {
						if (answer.qtype() == QType::A) {
							uint8_t ipv6[16];
							answer.qtype(QType::AAAA);
							server_.synth(answer.rdata(), ipv6);
							answer.rdata(ipv6, 16);
						}
					}
					if (sendto(server_.sock6fd_, answer.get(), apacket.len_, 0, (struct sockaddr*) &sender_, sizeof(sender_)) == -1) {
						syslog(LOG_DAEMON | LOG_ERR, "Can't send response: sendto failure: %d (%s)", errno, strerror(errno));
					}
			} else {
				if (sendto(server_.sock6fd_, answer.get(), res, 0, (struct sockaddr*) &sender_, sizeof(sender_)) == -1) {
					syslog(LOG_DAEMON | LOG_ERR, "Can't send response: sendto failure: %d (%s)", errno, strerror(errno));
				}
			}
		} catch (std::exception& e) {
			syslog(LOG_DAEMON | LOG_ERR, "%s", e.what());
		}
	}
}
