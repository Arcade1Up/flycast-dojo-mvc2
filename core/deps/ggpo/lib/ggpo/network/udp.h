/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _UDP_H
#define _UDP_H

#include <string>
#include <utility>
#include "ggpo_poll.h"
#include "udp_msg.h"
#include "ggponet.h"

#define MAX_UDP_ENDPOINTS     16

constexpr size_t MAX_UDP_PACKET_SIZE = sizeof(UdpMsg);

class Udp : public IPollSink {
 public:
  struct Stats {
    int bytes_sent;
    int packets_sent;
    float kbps_sent;
  };

  struct Callbacks {
    virtual ~Callbacks() {}
    virtual void OnMsg(sockaddr_in &from, UdpMsg *msg, int len) = 0;
  };

 protected:
  void Log(const char *fmt, ...);

 public:
  Udp();

  void initialize(uint16 port, Poll *p, Callbacks *callbacks);
  void sendTo(char *buffer, int len, int flags, struct sockaddr *dst, int destlen);
  bool OnLoopPoll(void *cookie) override;

  void setDstAddr(std::string dst_ip, uint16_t port);
  void callOnMsg(UdpMsg *msg, int len) { _callbacks->OnMsg(peer_addr_, msg, len); }

 public:
  ~Udp() override;

 protected:
  // Network transmission information
  SOCKET _socket;

  // state management
  Callbacks *_callbacks;

  // destination
  sockaddr_in peer_addr_;
};

#endif
