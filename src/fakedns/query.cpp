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

#include "query.h"
#include "server.h"
#include <algorithm>
#include <arpa/inet.h>
#include <net/if.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Query::Query(uint8_t *data, size_t len, int sock6fd, struct sockaddr_in6 sender,
             socklen_t sender_slen, Server &server)
    : data_{data}, len_{len}, sock6fd_{sock6fd},
      sender_slen_{sender_slen}, server_{server} {
  sender_ = sender;
}

Query::Query(const Query &rhs)
    : data_{rhs.data_}, len_{rhs.len_},
      sender_slen_{rhs.sender_slen_}, server_{rhs.server_} {
  sender_ = rhs.sender_;
}

Query::Query(Query &&rhs)
    : data_{rhs.data_}, len_{rhs.len_},
      sender_slen_{rhs.sender_slen_}, server_{rhs.server_} {
  sender_ = rhs.sender_;
  rhs.data_ = nullptr;
}

Query::~Query() {}

void Query::operator()() {
  DNSHeader *header = (DNSHeader *)data_;
  uint8_t answer_data[Config::response_maxlength_];
  size_t answer_len;
  char buffer[Config::response_maxlength_];
  uint8_t ip[4];
  if (header->qr() == 0 && header->opcode() == DNSHeader::OpCode::Query) {
    try {
      /* Parse the query */
      DNSPacket packet{data_, len_, (size_t)Config::response_maxlength_};
      /* Parse the question label */
      packet.question_[0].name_.toString(buffer, sizeof(buffer));
      if (sscanf(buffer, "%hhu-%hhu-%hhu-%hhu.dns64perf.test.", ip, ip + 1,
                 ip + 2, ip + 3) != 4) {
        syslog(LOG_DAEMON | LOG_INFO, "Received unparsable query: %s", buffer);
        return;
      }
      /* Creating base answer */
      memset(answer_data, 0x00, sizeof(answer_data));
      DNSHeader *header = reinterpret_cast<DNSHeader *>(answer_data);
      header->id(packet.header_->id());
      header->qr(1);
      header->opcode(DNSHeader::OpCode::Query);
      header->aa(false);
      header->tc(false);
      header->rd(true);
      header->ra(false);
      header->rcode(DNSHeader::RCODE::NoError);
      header->qdcount(1);
      header->ancount(0);
      header->nscount(0);
      header->arcount(0);
      answer_len = sizeof(DNSHeader);
      /* Copy question */
      memcpy(answer_data + answer_len, packet.question_[0].begin_,
             packet.question_[0].size());
      answer_len += packet.question_[0].size();
      if (packet.question_[0].qtype() == QType::A) {
        /* Add answer */
        header->ancount(1);
        /* Add answer label */
        uint16_t *ptr = reinterpret_cast<uint16_t *>(answer_data + answer_len);
        *ptr = htons((packet.question_[0].begin_ - packet.begin_) | 0xc000);
        answer_len += 2;
        uint16_t *qtype =
            reinterpret_cast<uint16_t *>(answer_data + answer_len);
        *qtype = htons(QType::A);
        answer_len += 2;
        uint16_t *qclass =
            reinterpret_cast<uint16_t *>(answer_data + answer_len);
        *qclass = htons(QClass::IN);
        answer_len += 2;
        uint32_t *ttl = reinterpret_cast<uint32_t *>(answer_data + answer_len);
        *ttl = htonl(0);
        answer_len += 4;
        uint16_t *rdlength =
            reinterpret_cast<uint16_t *>(answer_data + answer_len);
        *rdlength = htons(4);
        answer_len += 2;
        uint8_t *rdata = answer_data + answer_len;
        memcpy(rdata, ip, 4);
        answer_len += 4;
      } else if (packet.question_[0].qtype() == QType::AAAA) {
        /* Add answer */
        if (server_.config_.aaaa_mode_ == Config::aaaaMode::YES ||
            (server_.config_.aaaa_mode_ == Config::aaaaMode::PROBABILITY &&
             ((double)rand() / RAND_MAX) <=
                 server_.config_.aaaa_probability_)) {
          header->ancount(1);
          /* Add answer label */
          uint16_t *ptr =
              reinterpret_cast<uint16_t *>(answer_data + answer_len);
          *ptr = htons((packet.question_[0].begin_ - packet.begin_) | 0xc000);
          answer_len += 2;
          uint16_t *qtype =
              reinterpret_cast<uint16_t *>(answer_data + answer_len);
          *qtype = htons(QType::AAAA);
          answer_len += 2;
          uint16_t *qclass =
              reinterpret_cast<uint16_t *>(answer_data + answer_len);
          *qclass = htons(QClass::IN);
          answer_len += 2;
          uint32_t *ttl =
              reinterpret_cast<uint32_t *>(answer_data + answer_len);
          *ttl = htonl(0);
          answer_len += 4;
          uint16_t *rdlength =
              reinterpret_cast<uint16_t *>(answer_data + answer_len);
          *rdlength = htons(16);
          answer_len += 2;
          uint8_t *rdata = answer_data + answer_len;
          server_.synth(ip, rdata);
          answer_len += 16;
        }
      }
      /* Send answer */
      if (sendto(sock6fd_, answer_data, answer_len, 0,
                 (struct sockaddr *)&sender_, sizeof(sender_)) == -1) {
        syslog(LOG_DAEMON | LOG_ERR,
               "Can't send response: sendto failure: %d (%s)", errno,
               strerror(errno));
      }
    } catch (std::exception &e) {
      syslog(LOG_DAEMON | LOG_ERR, "%s", e.what());
    }
  }
}
