/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "udp-echo-client.h"
#include "ns3/ndn-interest.h" // edit
#include "ns3/names.h"  // edit
#include "ns3/ndn-content-object.h" // edit
#include "ns3/ipv4.h" // edit
#include <stdlib.h> // edit
#include <stdio.h> // edit
#include "ns3/string.h" // edit

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpEchoClientApplication");
NS_OBJECT_ENSURE_REGISTERED (UdpEchoClient);

TypeId
UdpEchoClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpEchoClient")
    .SetParent<Application> ()
    .AddConstructor<UdpEchoClient> ()
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&UdpEchoClient::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval",
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&UdpEchoClient::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&UdpEchoClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort",
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&UdpEchoClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&UdpEchoClient::SetDataSize,
                                         &UdpEchoClient::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SimName","Name of Simulation",
                   StringValue ("simulation"),
                   MakeStringAccessor (&UdpEchoClient::m_simName),
                   MakeStringChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&UdpEchoClient::m_txTrace))
  ;
  return tid;
}

UdpEchoClient::UdpEchoClient ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_received = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  m_data = 0;
  m_dataSize = 0;
}

UdpEchoClient::~UdpEchoClient()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;

  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
}

void
UdpEchoClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void
UdpEchoClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void
UdpEchoClient::SetRemote (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void
UdpEchoClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
UdpEchoClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  int m_nodeId = GetNode ()->GetId ();
  ueFileTx << m_simName << "_" << m_nodeId << "_TX.csv";
  ueFileRx << m_simName << "_" << m_nodeId << "_RX.csv";


  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          m_socket->Bind();
          m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
          m_socket->Bind6();
          m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpEchoClient::HandleRead, this));

  ScheduleTransmit (Seconds (0.));
}

void
UdpEchoClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

void
UdpEchoClient::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so
  // neither will we.
  //
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t
UdpEchoClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void
UdpEchoClient::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (this << fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void
UdpEchoClient::SetFill (uint8_t fill, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void
UdpEchoClient::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      m_size = dataSize;
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void
UdpEchoClient::ScheduleTransmit (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &UdpEchoClient::Send, this);
}

void
UdpEchoClient::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> packet;
  ns3::ndn::Interest interestHeader;

  Ptr<ns3::ndn::Name> nameWithSequence = Create<ns3::ndn::Name> ("/video");
  (*nameWithSequence) (m_sent);

  interestHeader.SetName                (nameWithSequence);

  packet = Create<Packet> ();
  packet->AddHeader (interestHeader);

  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (packet);
  m_socket->Send (packet);

  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("Node ID: " << GetNode()->GetId ());
      NS_LOG_INFO ("UE Address: " << GetNode()->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal());
      NS_LOG_INFO ("Tx time: " << Simulator::Now ().GetSeconds () << "s");
      NS_LOG_INFO ("Interest name: " << interestHeader.GetName());
      NS_LOG_INFO ("Interest size: " << packet->GetSize ());
      NS_LOG_INFO ("IP dest: " << Ipv4Address::ConvertFrom (m_peerAddress));
      NS_LOG_INFO ("Port: " << m_peerPort);

      ofstream out ((ueFileTx.str ()).c_str (), ios::app);
//      out << " Sequence Number: " << m_sent <<
//             " Tx time (s) " << Simulator::Now ().GetSeconds () <<
//             " Interest name: " << interestHeader.GetName() <<
//             " Interest size (Byte): " << packet->GetSize () <<
//             " IP source: " << GetNode()->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal() <<
//             " IP dest: " << Ipv4Address::ConvertFrom (m_peerAddress) <<
//             " Port: " << m_peerPort << endl;
      out << m_sent << "\t";
      std::list<std::string> myList = interestHeader.GetName().GetComponents();
      std::list<std::string>::iterator iter;
      int i = 0;
      for (iter=myList.begin(); iter!=myList.end(); iter++)
        {
           if (i == 1)
             out << *iter << "\t";
           i++;
        }
      out << Simulator::Now ().GetSeconds () << "\t";
      out << packet->GetSize () << "\t";
      out << GetNode()->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal() << "\n";

    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv6Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }

  ++m_sent;

  if (m_sent < m_count)
    {
      ScheduleTransmit (m_interval);
    }
}

void
UdpEchoClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      Ptr<Packet> pCopy = packet->Copy ();
      Ptr<ns3::ndn::ContentObject> contentObjectHeader = Create<ns3::ndn::ContentObject> ();
      pCopy->RemoveHeader (*contentObjectHeader);

      NS_LOG_INFO ("Node ID: " << GetNode()->GetId ());
      NS_LOG_INFO ("Node Address: " << GetNode()->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal());
      NS_LOG_INFO ("Rx time: " << Simulator::Now ().GetSeconds () << "s");
      NS_LOG_INFO ("Content name: " << contentObjectHeader->GetName());
      NS_LOG_INFO ("Content size (Byte): " << packet->GetSize ());
      NS_LOG_INFO ("Source: " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
      NS_LOG_INFO ("Port: " << InetSocketAddress::ConvertFrom (from).GetPort ());

      ofstream out ((ueFileRx.str ()).c_str (), ios::app);
//      out << " Sequence number: " << m_received <<
//             " Rx time (s) " << Simulator::Now ().GetSeconds () <<
//             " Content name: " << contentObjectHeader->GetName() <<
//             " Content size (Byte): " << packet->GetSize () <<
//             " Source: " << InetSocketAddress::ConvertFrom (from).GetIpv4 () <<
//             " Port: " << InetSocketAddress::ConvertFrom (from).GetPort () <<
//             " UE Address: " << GetNode()->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal() << endl;
      out << m_received << "\t";
      std::list<std::string> myList = contentObjectHeader->GetName().GetComponents();
      std::list<std::string>::iterator iter;
      int i = 0;
      for (iter=myList.begin(); iter!=myList.end(); iter++)
        {
           if (i == 1)
             out << *iter << "\t";
           i++;
        }
      out << Simulator::Now ().GetSeconds () << "\t";
      out << packet->GetSize () << "\t";
      out << GetNode()->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal() << "\n";


      ++m_received;
    }
}


} // Namespace ns3
