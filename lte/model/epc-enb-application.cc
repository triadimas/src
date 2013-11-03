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


#include "epc-enb-application.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/uinteger.h"
#include "ns3/ndn-interest.h" // edit
#include "ns3/ndn-content-object.h" // edit
#include "ns3/udp-header.h" // edit

#include "epc-gtpu-header.h"
#include "eps-bearer-tag.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EpcEnbApplication");


EpcEnbApplication::EpsFlowId_t::EpsFlowId_t ()
{
}

EpcEnbApplication::EpsFlowId_t::EpsFlowId_t (const uint16_t a, const uint8_t b)
  : m_rnti (a),
    m_bid (b)
{
}

bool
operator == (const EpcEnbApplication::EpsFlowId_t &a, const EpcEnbApplication::EpsFlowId_t &b)
{
  return ( (a.m_rnti == b.m_rnti) && (a.m_bid == b.m_bid) );
}

bool
operator < (const EpcEnbApplication::EpsFlowId_t& a, const EpcEnbApplication::EpsFlowId_t& b)
{
  return ( (a.m_rnti < b.m_rnti) || ( (a.m_rnti == b.m_rnti) && (a.m_bid < b.m_bid) ) );
}


TypeId
EpcEnbApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EpcEnbApplication")
    .SetParent<Object> ();
  return tid;
}

void
EpcEnbApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_lteSocket = 0;
  m_s1uSocket = 0;
  delete m_s1SapProvider;
  delete m_s1apSapEnb;
}


EpcEnbApplication::EpcEnbApplication (Ptr<Socket> lteSocket, Ptr<Socket> s1uSocket, Ipv4Address enbS1uAddress, Ipv4Address sgwS1uAddress, uint16_t cellId)
  : m_lteSocket (lteSocket),
    m_s1uSocket (s1uSocket),
    m_enbS1uAddress (enbS1uAddress),
    m_sgwS1uAddress (sgwS1uAddress),
    m_gtpuUdpPort (2152), // fixed by the standard
    m_s1SapUser (0),
    m_s1apSapMme (0),
    m_cellId (cellId)
{
  NS_LOG_FUNCTION (this << lteSocket << s1uSocket << sgwS1uAddress);
  m_s1uSocket->SetRecvCallback (MakeCallback (&EpcEnbApplication::RecvFromS1uSocket, this));
  m_lteSocket->SetRecvCallback (MakeCallback (&EpcEnbApplication::RecvFromLteSocket, this));
  m_s1SapProvider = new MemberEpcEnbS1SapProvider<EpcEnbApplication> (this);
  m_s1apSapEnb = new MemberEpcS1apSapEnb<EpcEnbApplication> (this);
}

EpcEnbApplication::Buff_t::Buff_t()  // new
{
}

EpcEnbApplication::~EpcEnbApplication (void)
{
  NS_LOG_FUNCTION (this);
}


void
EpcEnbApplication::SetS1SapUser (EpcEnbS1SapUser * s)
{
  m_s1SapUser = s;
}


EpcEnbS1SapProvider*
EpcEnbApplication::GetS1SapProvider ()
{
  return m_s1SapProvider;
}

void
EpcEnbApplication::SetS1apSapMme (EpcS1apSapMme * s)
{
  m_s1apSapMme = s;
}


EpcS1apSapEnb*
EpcEnbApplication::GetS1apSapEnb ()
{
  return m_s1apSapEnb;
}

void
EpcEnbApplication::DoInitialUeMessage (uint64_t imsi, uint16_t rnti)
{
  NS_LOG_FUNCTION (this);
  // side effect: create entry if not exist
  m_imsiRntiMap[imsi] = rnti;
  m_s1apSapMme->InitialUeMessage (imsi, rnti, imsi, m_cellId);
}

std::map<ns3::ndn::Name, std::vector<EnbPitFace_t> > EpcEnbApplication::GetNameFaceMap () // new
{
    return m_nameFaceMap;
}

std::map<ns3::ndn::Name, CsEps_t> EpcEnbApplication::GetNameContentMap () // new
{
    return m_nameContentMap;
}

void EpcEnbApplication::SetNameContentMap (CsEps_t cs, ns3::ndn::Name name)  // new
{
    m_nameContentMap[name] = cs;
}

void EpcEnbApplication::SetNameFaceMap (EnbPitFace_t pitFace, ns3::ndn::Name name) // new
{
    m_nameFaceMap[name].push_back(pitFace);
}

void EpcEnbApplication::ErasePitEntry (ns3::ndn::Name name) // new
{
    m_nameFaceMap.erase (name);
}

void
EpcEnbApplication::DoPathSwitchRequest (EpcEnbS1SapProvider::PathSwitchRequestParameters params)
{
  NS_LOG_FUNCTION (this);
  uint16_t enbUeS1Id = params.rnti;
  uint64_t mmeUeS1Id = params.mmeUeS1Id;
  uint64_t imsi = mmeUeS1Id;
  // side effect: create entry if not exist
  m_imsiRntiMap[imsi] = params.rnti;

  uint16_t gci = params.cellId;
  std::list<EpcS1apSapMme::ErabSwitchedInDownlinkItem> erabToBeSwitchedInDownlinkList;
  for (std::list<EpcEnbS1SapProvider::BearerToBeSwitched>::iterator bit = params.bearersToBeSwitched.begin ();
       bit != params.bearersToBeSwitched.end ();
       ++bit)
    {
      EpsFlowId_t flowId;
      flowId.m_rnti = params.rnti;
      flowId.m_bid = bit->epsBearerId;
      uint32_t teid = bit->teid;

      EpsFlowId_t rbid (params.rnti, bit->epsBearerId);
      // side effect: create entries if not exist
      m_rbidTeidMap[params.rnti][bit->epsBearerId] = teid;
      m_teidRbidMap[teid] = rbid;

      EpcS1apSapMme::ErabSwitchedInDownlinkItem erab;
      erab.erabId = bit->epsBearerId;
      erab.enbTransportLayerAddress = m_enbS1uAddress;
      erab.enbTeid = bit->teid;

      erabToBeSwitchedInDownlinkList.push_back (erab);
    }
  m_s1apSapMme->PathSwitchRequest (enbUeS1Id, mmeUeS1Id, gci, erabToBeSwitchedInDownlinkList);
}

void
EpcEnbApplication::DoUeContextRelease (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  std::map<uint16_t, std::map<uint8_t, uint32_t> >::iterator rntiIt = m_rbidTeidMap.find (rnti);
  if (rntiIt != m_rbidTeidMap.end ())
    {
      for (std::map<uint8_t, uint32_t>::iterator bidIt = rntiIt->second.begin ();
           bidIt != rntiIt->second.end ();
           ++bidIt)
        {
          uint32_t teid = bidIt->second;
          m_teidRbidMap.erase (teid);
        }
      m_rbidTeidMap.erase (rntiIt);
    }
}

void
EpcEnbApplication::DoInitialContextSetupRequest (uint64_t mmeUeS1Id, uint16_t enbUeS1Id, std::list<EpcS1apSapEnb::ErabToBeSetupItem> erabToBeSetupList)
{
  NS_LOG_FUNCTION (this);

  for (std::list<EpcS1apSapEnb::ErabToBeSetupItem>::iterator erabIt = erabToBeSetupList.begin ();
       erabIt != erabToBeSetupList.end ();
       ++erabIt)
    {
      // request the RRC to setup a radio bearer

      uint64_t imsi = mmeUeS1Id;
      std::map<uint64_t, uint16_t>::iterator imsiIt = m_imsiRntiMap.find (imsi);
      NS_ASSERT_MSG (imsiIt != m_imsiRntiMap.end (), "unknown IMSI");
      uint16_t rnti = imsiIt->second;

      struct EpcEnbS1SapUser::DataRadioBearerSetupRequestParameters params;
      params.rnti = rnti;
      params.bearer = erabIt->erabLevelQosParameters;
      params.bearerId = erabIt->erabId;
      params.gtpTeid = erabIt->sgwTeid;
      m_s1SapUser->DataRadioBearerSetupRequest (params);

      EpsFlowId_t rbid (rnti, erabIt->erabId);
      // side effect: create entries if not exist
      m_rbidTeidMap[rnti][erabIt->erabId] = params.gtpTeid;
      m_teidRbidMap[params.gtpTeid] = rbid;

    }
}

void
EpcEnbApplication::DoPathSwitchRequestAcknowledge (uint64_t enbUeS1Id, uint64_t mmeUeS1Id, uint16_t gci, std::list<EpcS1apSapEnb::ErabSwitchedInUplinkItem> erabToBeSwitchedInUplinkList)
{
  NS_LOG_FUNCTION (this);

  uint64_t imsi = mmeUeS1Id;
  std::map<uint64_t, uint16_t>::iterator imsiIt = m_imsiRntiMap.find (imsi);
  NS_ASSERT_MSG (imsiIt != m_imsiRntiMap.end (), "unknown IMSI");
  uint16_t rnti = imsiIt->second;
  EpcEnbS1SapUser::PathSwitchRequestAcknowledgeParameters params;
  params.rnti = rnti;
  m_s1SapUser->PathSwitchRequestAcknowledge (params);
}

void
EpcEnbApplication::RecvFromLteSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (socket == m_lteSocket);
  Ptr<Packet> packet = socket->Recv ();

  // workaround for bug 231 https://www.nsnam.org/bugzilla/show_bug.cgi?id=231
  SocketAddressTag satag;
  packet->RemovePacketTag (satag);

  EpsBearerTag tag;
  bool found = packet->RemovePacketTag (tag);
  NS_ASSERT (found);
  uint16_t rnti = tag.GetRnti ();
  uint8_t bid = tag.GetBid ();
  NS_LOG_LOGIC ("received packet with RNTI=" << (uint32_t) rnti << ", BID=" << (uint32_t)  bid);

  std::map<uint16_t, std::map<uint8_t, uint32_t> >::iterator rntiIt = m_rbidTeidMap.find (rnti);
  if (rntiIt == m_rbidTeidMap.end ())
    {
      NS_LOG_WARN ("UE context not found, discarding packet");
      //SendToLteSocket (packet, rnti, bid);
    }
  else
    {
      // getting interest, ip, and udp headers
      Ptr<Packet> pCopy = packet->Copy ();
      Ipv4Header ipv4Header;
      pCopy->RemoveHeader (ipv4Header);

      if (ipv4Header.GetDestination() == Ipv4Address ("192.168.1.5"))
      {
          NS_LOG_INFO ("THIS IS BACKGROUND TRAFFIC");
      }
      else
      {

      UdpHeader udpHeader;
      pCopy->RemoveHeader (udpHeader);
      ns3::ndn::Interest interestHeader;
      pCopy->RemoveHeader(interestHeader);

      // checking CS
      std::map<ns3::ndn::Name, CsEps_t>::iterator contentNameIt = m_nameContentMap.find (interestHeader.GetName());

      NS_LOG_INFO ("Checking CS table. Interest name: " << interestHeader.GetName());
      NS_LOG_INFO ("UE IP Address: " << ipv4Header.GetSource());

      if (contentNameIt == m_nameContentMap.end ()) // no match is found in CS
      {
        NS_LOG_INFO ("No match is found in CS -> Checking PIT table");
        // checking PIT
        std::map<ns3::ndn::Name, std::vector<EnbPitFace_t> >::iterator interestNameIt
        = m_nameFaceMap.find (interestHeader.GetName());

        EnbPitFace_t pitFace (rnti, bid, udpHeader.GetSourcePort(), ipv4Header.GetSource());

        if (interestNameIt == m_nameFaceMap.end ())  // no match is found in PIT
        {
          NS_LOG_INFO ("No match is found in PIT. Adding a new PIT entry and sending the Interest to SGW/PGW");
          // inserting new entry to the PIT map
          m_nameFaceMap[interestHeader.GetName()].push_back(pitFace);

          std::map<uint8_t, uint32_t>::iterator bidIt = rntiIt->second.find (bid);
          NS_ASSERT (bidIt != rntiIt->second.end ());
          uint32_t teid = bidIt->second;
          SendToS1uSocket (packet, teid);
        }
        else  // a match is found in PIT
        {
          NS_LOG_INFO ("A match is founf in PIT. Adding a new face into face list");
          // inserting new face into face list
          m_nameFaceMap[interestHeader.GetName()].push_back(pitFace);
        }
      }
      else
      {
        NS_LOG_INFO ("A match is found in CS. Sending content to UE");
        // getting the packet to be forwarded from CS
        CsEps_t cs = contentNameIt->second;
        Ptr<Packet> packetForUe = Create<Packet> (1316); // 7 PES @188Bytes
        packetForUe->AddHeader(*(cs.m_contentHeader));
        cs.m_udpHeader.SetDestinationPort(udpHeader.GetSourcePort());
        packetForUe->AddHeader(cs.m_udpHeader);
        cs.m_ipHeader.SetDestination(ipv4Header.GetSource());
        packetForUe->AddHeader(cs.m_ipHeader);

//        if (m_vbuff.size() > 0)
//        {
//           m_vbuff.erase(m_vbuff.begin(),m_vbuff.end());
//        }
//
//        Buff_t packetBuffer;
//        packetBuffer.m_packet = packetForUe->Copy ();
//        packetBuffer.m_bid    = bid;
//        m_vbuff.push_back(packetBuffer);

        Buff_t packetBuffer;
        packetBuffer.m_packet = packetForUe->Copy ();
        packetBuffer.m_bid = bid;

        std::map<uint16_t, Buff_t>::iterator buffIt = m_buffMap.find (rnti);

        if (buffIt == m_buffMap.end ())
        {
          m_buffMap[rnti] = packetBuffer;
        }
        else
        {
          m_buffMap.erase (buffIt);
          m_buffMap[rnti] = packetBuffer;
        }

        // sending packet
        SendToLteSocket (packetForUe, rnti, bid);
      }
     }
    }
}

void
EpcEnbApplication::RecvFromS1uSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_ASSERT (socket == m_s1uSocket);
  Ptr<Packet> packet = socket->Recv ();
  Ptr<Packet> tmp = packet->Copy();
  GtpuHeader gtpu;
  packet->RemoveHeader (gtpu);
  uint32_t teid = gtpu.GetTeid ();
  std::map<uint32_t, EpsFlowId_t>::iterator it = m_teidRbidMap.find (teid);

  if (it == m_teidRbidMap.end ())
  {
      //NS_ASSERT (it != m_teidRbidMap.end ());
      NS_LOG_INFO ("PACKET SIZE: " << tmp->GetSize());
      Ipv4Header ipv4Header;
      tmp->RemoveHeader (ipv4Header);
      NS_LOG_INFO ("CC MESSAGE IS RECEIVED. CONFIGURING PIT TABLE IN:" << ipv4Header.GetDestination());
  }
  else
  {
  // workaround for bug 231 https://www.nsnam.org/bugzilla/show_bug.cgi?id=231
  SocketAddressTag tag;
  packet->RemovePacketTag (tag);

  // getting ip, and udp headers
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
  NS_LOG_INFO ("UE IP Address: " << ipv4Header.GetDestination());

  if (contentNameIt == m_nameContentMap.end ()) // no match is found in CS
  {
    NS_LOG_INFO ("No match is found in CS -> Checking PIT");
    // checking PIT
    std::map<ns3::ndn::Name, std::vector<EnbPitFace_t> >::iterator interestNameIt
    = m_nameFaceMap.find (contentHeader->GetName());

    if (interestNameIt == m_nameFaceMap.end ())  // no match is found in PIT
    {
        NS_LOG_WARN ("No match is found in PIT. Discarding packet");
    }
    else // a match is found in PIT
    {
        NS_LOG_INFO ("A match is found in PIT");
        // caching content
        CsEps_t cs;
        cs.m_content       = pCopy;
        cs.m_contentHeader = contentHeader;
        cs.m_ipHeader      = ipv4Header;
        cs.m_udpHeader     = udpHeader;

        m_nameContentMap[contentHeader->GetName()] = cs;
        // composing packet
        std::vector<EnbPitFace_t> tmp_pitFace = interestNameIt->second;

        for (uint32_t i = 0; i < tmp_pitFace.size(); i++)
        {
            Ptr<Packet> p = packet->Copy ();
            NS_LOG_INFO ("Generating packet");
            NS_LOG_INFO ("Destination of Packet: " << tmp_pitFace[i].m_ipv4address << " BID: " << (uint32_t) (tmp_pitFace[i].m_bid));
            ipv4Header.SetDestination(tmp_pitFace[i].m_ipv4address);
            udpHeader.SetDestinationPort(tmp_pitFace[i].m_port);
            p->AddHeader(udpHeader);
            p->AddHeader(ipv4Header);

            Buff_t packetBuffer;
            packetBuffer.m_packet = p->Copy ();
            packetBuffer.m_bid = tmp_pitFace[i].m_bid;

            std::map<uint16_t, Buff_t>::iterator buffIt = m_buffMap.find (tmp_pitFace[i].m_rnti);

            if (buffIt == m_buffMap.end ())
            {
               m_buffMap[tmp_pitFace[i].m_rnti] = packetBuffer;
            }
            else
            {
               m_buffMap.erase (buffIt);
               m_buffMap[tmp_pitFace[i].m_rnti] = packetBuffer;
            }

            SendToLteSocket (p, tmp_pitFace[i].m_rnti, tmp_pitFace[i].m_bid);
        }

        m_nameFaceMap.erase (interestNameIt);
        NS_LOG_INFO ("PIT entry is deleted");
    }
  }
  else // if a match is found in CS
    {
        NS_LOG_WARN ("No match is found in CS. Discarding packet");
    }
  }
}


void
EpcEnbApplication::SendToLteSocket (Ptr<Packet> packet, uint16_t rnti, uint8_t bid)
{
  NS_LOG_FUNCTION (this << packet << rnti << (uint16_t) bid << packet->GetSize ());
  EpsBearerTag tag (rnti, bid);
  packet->AddPacketTag (tag);
  int sentBytes = m_lteSocket->Send (packet);
  NS_ASSERT (sentBytes > 0);
}

void
EpcEnbApplication::SendToCloudComponent (Ptr<Packet> packet)
{
  NS_LOG_INFO ("Send Handover Information to Cloud Components");
  uint32_t flags   = 0;
  uint16_t UdpPort = 1234;
  m_s1uSocket->SendTo (packet, flags, InetSocketAddress(Ipv4Address ("13.0.0.1"), UdpPort));
}


void
EpcEnbApplication::SendToS1uSocket (Ptr<Packet> packet, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << teid <<  packet->GetSize ());
  GtpuHeader gtpu;
  gtpu.SetTeid (teid);
  // From 3GPP TS 29.281 v10.0.0 Section 5.1
  // Length of the payload + the non obligatory GTP-U header
  gtpu.SetLength (packet->GetSize () + gtpu.GetSerializedSize () - 8);
  packet->AddHeader (gtpu);
  uint32_t flags = 0;
  m_s1uSocket->SendTo (packet, flags, InetSocketAddress(m_sgwS1uAddress, m_gtpuUdpPort));
}


}; // namespace ns3
