#include "lte-ccn-common.h"


namespace ns3 {

EnbPitFace_t::EnbPitFace_t()
{
}

EnbPitFace_t::EnbPitFace_t(const uint16_t rnti, const uint8_t bid, const uint16_t port, const Ipv4Address addr)
  : m_rnti (rnti),
    m_bid (bid),
    m_port (port),
    m_ipv4address(addr)

{
}

SgwPgwPitFace_t::SgwPgwPitFace_t()
{
}

SgwPgwPitFace_t::SgwPgwPitFace_t(const uint16_t port, const Ipv4Address addr)
  : m_port (port),
    m_ipv4address(addr)

{
}

CsEps_t::CsEps_t()
{
}

};
