/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jaume Nin <jnin@cttc.cat>
 *         Nicola Baldo <nbaldo@cttc.cat>
 */


#include "epc-sgw-pgw-application.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/epc-gtpu-header.h"
#include "ns3/abort.h"
#include "ns3/ndn-interest.h" // edit
#include "ns3/ndn-content-object.h" // edit
#include "ns3/udp-header.h" // edit


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EpcSgwPgwApplication");


/////////////////////////
// UeInfo
/////////////////////////


EpcSgwPgwApplication::UeInfo::UeInfo ()
{
  NS_LOG_FUNCTION (this);
}

void
EpcSgwPgwApplication::UeInfo::AddBearer (Ptr<EpcTft> tft, uint8_t bearerId, uint32_t teid)
{
  NS_LOG_FUNCTION (this << tft << teid);
  m_teidByBearerIdMap[bearerId] = teid;
  return m_tftClassifier.Add (tft, teid);
}

uint32_t
EpcSgwPgwApplication::UeInfo::Classify (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  // we hardcode DOWNLINK direction since the PGW is espected to
  // classify only downlink packets (uplink packets will go to the
  // internet without any classification).
  return m_tftClassifier.Classify (p, EpcTft::DOWNLINK);
}

Ipv4Address
EpcSgwPgwApplication::UeInfo::GetEnbAddr ()
{
  return m_enbAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetEnbAddr (Ipv4Address enbAddr)
{
  m_enbAddr = enbAddr;
}

Ipv4Address
EpcSgwPgwApplication::UeInfo::GetUeAddr ()
{
  return m_ueAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetUeAddr (Ipv4Address ueAddr)
{
  m_ueAddr = ueAddr;
}

/////////////////////////
// EpcSgwPgwApplication
/////////////////////////


TypeId
EpcSgwPgwApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EpcSgwPgwApplication")
    .SetParent<Object> ();
  return tid;
}

void
EpcSgwPgwApplication::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_s1uSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  m_s1uSocket = 0;
  delete (m_s11SapSgw);
}



EpcSgwPgwApplication::EpcSgwPgwApplication (const Ptr<VirtualNetDevice> tunDevice, const Ptr<Socket> s1uSocket)
  : m_s1uSocket (s1uSocket),
    m_tunDevice (tunDevice),
    m_gtpuUdpPort (2152), // fixed by the standard
    m_teidCount (0),
    m_s11SapMme (0)
{
  NS_LOG_FUNCTION (this << tunDevice << s1uSocket);
  m_s1uSocket->SetRecvCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromS1uSocket, this));
  m_s11SapSgw = new MemberEpcS11SapSgw<EpcSgwPgwApplication> (this);
}


EpcSgwPgwApplication::~EpcSgwPgwApplication ()
{
  NS_LOG_FUNCTION (this);
}


bool
EpcSgwPgwApplication::RecvFromTunDevice (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << source << dest << packet << packet->GetSize ());

  // get IP address of UE
  Ipv4Header ipv4Header;
  packet->RemoveHeader (ipv4Header);
  UdpHeader udpHeader;
  packet->RemoveHeader (udpHeader); // now the packet format is: content header+content

  Ptr<Packet> pCopy = packet->Copy ();
  Ptr<ns3::ndn::ContentObject> contentHeader = Create <ns3::ndn::ContentObject> ();
  pCopy->RemoveHeader(*contentHeader);

  // checking CS
  std::map<ns3::ndn::Name, CsEps_t>::iterator contentNameIt = m_nameContentMap.find (contentHeader->GetName());

  NS_LOG_INFO ("Checking CS table. Name of content: " << contentHeader->GetName());

  if (contentNameIt == m_nameContentMap.end ()) // no match is found in CS
  {
    NS_LOG_INFO ("No match is found in CS -> Checking PIT table");
    // checking PIT
    std::map<ns3::ndn::Name, std::vector<SgwPgwPitFace_t> >::iterator interestNameIt
    = m_nameFaceMap.find (contentHeader->GetName());

    if (interestNameIt == m_nameFaceMap.end ())  // no match is found in PIT
    {
        NS_LOG_INFO ("No match is found in PIT, this content is not requested, discarding packet");
    }
    else  // a match is found in PIT
    {
        NS_LOG_INFO ("A match is found in PIT. Caching content");
        // caching content
        CsEps_t cs;
        cs.m_content       = pCopy;
        cs.m_contentHeader = contentHeader;
        cs.m_ipHeader      = ipv4Header;
        cs.m_udpHeader     = udpHeader;

        m_nameContentMap[contentHeader->GetName()] = cs;
        // composing packet
        std::vector<SgwPgwPitFace_t> tmp_pitFace = interestNameIt->second;

        for (uint32_t i = 0; i < tmp_pitFace.size(); i++)
        {
            NS_LOG_INFO ("Generating packet");
            Ptr<Packet> p = packet->Copy ();
            ipv4Header.SetDestination(tmp_pitFace[i].m_ipv4address);
            udpHeader.SetDestinationPort(tmp_pitFace[i].m_port);
            p->AddHeader(udpHeader);
            p->AddHeader(ipv4Header);
            Ipv4Address ueAddr =  ipv4Header.GetDestination ();
            NS_LOG_LOGIC ("packet addressed to UE " << ueAddr);

            // find corresponding UeInfo address
            std::map<Ipv4Address, Ptr<UeInfo> >::iterator it = m_ueInfoByAddrMap.find (ueAddr);
            if (it == m_ueInfoByAddrMap.end ())
            {
                NS_LOG_WARN ("unknown UE address " << ueAddr) ;
            }
            else
            {
                Ipv4Address enbAddr = it->second->GetEnbAddr ();
                uint32_t teid = it->second->Classify (p);
                if (teid == 0)
                {
                    NS_LOG_WARN ("no matching bearer for this packet");
                }
                else
                {
                    NS_LOG_INFO ("Sending content to eNodeB");
                    SendToS1uSocket (p, enbAddr, teid);
                }
            }
        }

        m_nameFaceMap.erase (interestNameIt);
    }
  }
  else // if a match is found in CS
  {
      NS_LOG_WARN ("A match is found in CS. Discarding packet");
  }

  // there is no reason why we should notify the TUN
  // VirtualNetDevice that he failed to send the packet: if we receive
  // any bogus packet, it will just be silently discarded.
  const bool succeeded = true;
  return succeeded;
}

void
EpcSgwPgwApplication::RecvFromS1uSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_ASSERT (socket == m_s1uSocket);
  Ptr<Packet> packet = socket->Recv ();
  GtpuHeader gtpu;
  packet->RemoveHeader (gtpu);
  uint32_t teid = gtpu.GetTeid ();

  // workaround for bug 231 https://www.nsnam.org/bugzilla/show_bug.cgi?id=231
  SocketAddressTag tag;
  packet->RemovePacketTag (tag);

  // getting interest name
  Ptr<Packet> pCopy = packet->Copy ();
  Ipv4Header ipv4Header;
  pCopy->RemoveHeader (ipv4Header);
  UdpHeader udpHeader;
  pCopy->RemoveHeader (udpHeader);
  ns3::ndn::Interest interestHeader;
  pCopy->RemoveHeader(interestHeader);

  // checking CS
  std::map<ns3::ndn::Name, CsEps_t>::iterator contentNameIt = m_nameContentMap.find (interestHeader.GetName());

  NS_LOG_INFO ("Checking CS table. Interest name: " << interestHeader.GetName());

  if (contentNameIt == m_nameContentMap.end ()) // no match is found in CS
  {
    NS_LOG_INFO ("No match is found in CS table. Checking PIT table");
    // checking PIT
    std::map<ns3::ndn::Name, std::vector<SgwPgwPitFace_t> >::iterator interestNameIt
    = m_nameFaceMap.find (interestHeader.GetName());

    SgwPgwPitFace_t pitFace (udpHeader.GetSourcePort(), ipv4Header.GetSource());

    if (interestNameIt == m_nameFaceMap.end ())  // no match is found in PIT
    {
        NS_LOG_INFO ("No match is found in PIT. Installing a new PIT entry and sending the Interest");
        // inserting new entry to the PIT map
        m_nameFaceMap[interestHeader.GetName()].push_back(pitFace);

        SendToTunDevice (packet, teid);
    }
    else  // a match is found in PIT
    {
        NS_LOG_INFO ("A match is found in PIT. Adding a new face into face list");
        // inserting new face into face list
        m_nameFaceMap[interestHeader.GetName()].push_back(pitFace);
    }
  }
  else
  {
    NS_LOG_INFO ("A match is found in CS table");
    // getting the packet to be forwarded from CS
    CsEps_t cs = contentNameIt->second;
    Ptr<Packet> packetForUe = Create<Packet> (1316); // 7 PES @188Bytes
    packetForUe->AddHeader(*(cs.m_contentHeader));
    cs.m_udpHeader.SetDestinationPort(udpHeader.GetSourcePort());
    packetForUe->AddHeader(cs.m_udpHeader);
    cs.m_ipHeader.SetDestination(ipv4Header.GetSource());
    packetForUe->AddHeader(cs.m_ipHeader);

    // find corresponding UeInfo address
    std::map<Ipv4Address, Ptr<UeInfo> >::iterator it = m_ueInfoByAddrMap.find (ipv4Header.GetSource ());
    if (it == m_ueInfoByAddrMap.end ())
      {
        NS_LOG_WARN ("unknown UE address " << ipv4Header.GetSource ()) ;
      }
    else
      {
        Ipv4Address enbAddr = it->second->GetEnbAddr ();
        teid = it->second->Classify (packetForUe);
        if (teid == 0)
          {
            NS_LOG_WARN ("no matching bearer for this packet");
          }
        else
          {
            NS_LOG_INFO ("Sending packet to eNodeB");
            SendToS1uSocket (packetForUe, enbAddr, teid);
          }
      }
  }

//

}

void
EpcSgwPgwApplication::SendToTunDevice (Ptr<Packet> packet, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << teid);
  NS_LOG_LOGIC (" packet size: " << packet->GetSize () << " bytes");
  m_tunDevice->Receive (packet, 0x0800, m_tunDevice->GetAddress (), m_tunDevice->GetAddress (), NetDevice::PACKET_HOST);
}

void
EpcSgwPgwApplication::SendToS1uSocket (Ptr<Packet> packet, Ipv4Address enbAddr, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << enbAddr << teid);

  GtpuHeader gtpu;
  gtpu.SetTeid (teid);
  // From 3GPP TS 29.281 v10.0.0 Section 5.1
  // Length of the payload + the non obligatory GTP-U header
  gtpu.SetLength (packet->GetSize () + gtpu.GetSerializedSize () - 8);
  packet->AddHeader (gtpu);
  uint32_t flags = 0;
  m_s1uSocket->SendTo (packet, flags, InetSocketAddress(enbAddr, m_gtpuUdpPort));
}


void
EpcSgwPgwApplication::SetS11SapMme (EpcS11SapMme * s)
{
  m_s11SapMme = s;
}

EpcS11SapSgw*
EpcSgwPgwApplication::GetS11SapSgw ()
{
  return m_s11SapSgw;
}

void
EpcSgwPgwApplication::AddEnb (uint16_t cellId, Ipv4Address enbAddr, Ipv4Address sgwAddr)
{
  NS_LOG_FUNCTION (this << cellId << enbAddr << sgwAddr);
  EnbInfo enbInfo;
  enbInfo.enbAddr = enbAddr;
  enbInfo.sgwAddr = sgwAddr;
  m_enbInfoByCellId[cellId] = enbInfo;
}

void
EpcSgwPgwApplication::AddUe (uint64_t imsi)
{
  NS_LOG_FUNCTION (this << imsi);
  Ptr<UeInfo> ueInfo = Create<UeInfo> ();
  m_ueInfoByImsiMap[imsi] = ueInfo;
}

void
EpcSgwPgwApplication::SetUeAddress (uint64_t imsi, Ipv4Address ueAddr)
{
  NS_LOG_FUNCTION (this << imsi << ueAddr);
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);
  m_ueInfoByAddrMap[ueAddr] = ueit->second;
  ueit->second->SetUeAddr (ueAddr);
}

void
EpcSgwPgwApplication::DoCreateSessionRequest (EpcS11SapSgw::CreateSessionRequestMessage req)
{
  NS_LOG_FUNCTION (this << req.imsi);
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (req.imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << req.imsi);
  uint16_t cellId = req.uli.gci;
  std::map<uint16_t, EnbInfo>::iterator enbit = m_enbInfoByCellId.find (cellId);
  NS_ASSERT_MSG (enbit != m_enbInfoByCellId.end (), "unknown CellId " << cellId);
  Ipv4Address enbAddr = enbit->second.enbAddr;
  ueit->second->SetEnbAddr (enbAddr);

  EpcS11SapMme::CreateSessionResponseMessage res;
  res.teid = req.imsi; // trick to avoid the need for allocating TEIDs on the S11 interface

  for (std::list<EpcS11SapSgw::BearerContextToBeCreated>::iterator bit = req.bearerContextsToBeCreated.begin ();
       bit != req.bearerContextsToBeCreated.end ();
       ++bit)
    {
      // simple sanity check. If you ever need more than 4M teids
      // throughout your simulation, you'll need to implement a smarter teid
      // management algorithm.
      NS_ABORT_IF (m_teidCount == 0xFFFFFFFF);
      uint32_t teid = ++m_teidCount;
      ueit->second->AddBearer (bit->tft, bit->epsBearerId, teid);

      EpcS11SapMme::BearerContextCreated bearerContext;
      bearerContext.sgwFteid.teid = teid;
      bearerContext.sgwFteid.address = enbit->second.sgwAddr;
      bearerContext.epsBearerId =  bit->epsBearerId;
      bearerContext.bearerLevelQos = bit->bearerLevelQos;
      bearerContext.tft = bit->tft;
      res.bearerContextsCreated.push_back (bearerContext);
    }
  m_s11SapMme->CreateSessionResponse (res);

}

void
EpcSgwPgwApplication::DoModifyBearerRequest (EpcS11SapSgw::ModifyBearerRequestMessage req)
{
  NS_LOG_FUNCTION (this << req.teid);
  uint64_t imsi = req.teid; // trick to avoid the need for allocating TEIDs on the S11 interface
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);
  uint16_t cellId = req.uli.gci;
  std::map<uint16_t, EnbInfo>::iterator enbit = m_enbInfoByCellId.find (cellId);
  NS_ASSERT_MSG (enbit != m_enbInfoByCellId.end (), "unknown CellId " << cellId);
  Ipv4Address enbAddr = enbit->second.enbAddr;
  ueit->second->SetEnbAddr (enbAddr);
  // no actual bearer modification: for now we just support the minimum needed for path switch request (handover)
  EpcS11SapMme::ModifyBearerResponseMessage res;
  res.teid = imsi; // trick to avoid the need for allocating TEIDs on the S11 interface
  res.cause = EpcS11SapMme::ModifyBearerResponseMessage::REQUEST_ACCEPTED;
  m_s11SapMme->ModifyBearerResponse (res);
}

}; // namespace ns3
