
#ifndef LTE_CCN_COMMON_H
#define LTE_CCN_COMMON_H

#include "ns3/uinteger.h"
#include "ns3/ndn-content-object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/udp-header.h"
#include "ns3/ipv4-header.h"

namespace ns3 {


struct EnbPitFace_t
{
  uint16_t      m_rnti;
  uint8_t       m_bid;
  uint16_t      m_port;
  Ipv4Address   m_ipv4address;

public:
  EnbPitFace_t ();
  EnbPitFace_t (const uint16_t m_rnti, const uint8_t m_bid, const uint16_t m_port, const Ipv4Address addr);

};

struct SgwPgwPitFace_t
{
  uint16_t      m_port;
  Ipv4Address   m_ipv4address;

public:
  SgwPgwPitFace_t ();
  SgwPgwPitFace_t (const uint16_t m_port, const Ipv4Address addr);

};

struct CsEps_t
{
  Ptr<Packet> m_content;
  Ptr<ns3::ndn::ContentObject> m_contentHeader;
  Ipv4Header m_ipHeader;
  UdpHeader m_udpHeader;

public:
  CsEps_t ();
};


};

#endif
