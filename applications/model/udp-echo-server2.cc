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
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4-header.h" // edit
#include "ns3/x2-header.h" // new


#include "udp-echo-server2.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpEchoServerApplication2");
NS_OBJECT_ENSURE_REGISTERED (UdpEchoServer2);

TypeId
UdpEchoServer2::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpEchoServer2")
    .SetParent<Application> ()
    .AddConstructor<UdpEchoServer2> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&UdpEchoServer2::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("VmMigration",
                   "VM migration indicator",
                   UintegerValue (0), // 0: no VM migration;
                   MakeUintegerAccessor (&UdpEchoServer2::m_vmMigration),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

UdpEchoServer2::UdpEchoServer2 ()
{
  NS_LOG_FUNCTION (this);

  m_msgCounter = 0;
}

UdpEchoServer2::~UdpEchoServer2()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socket6 = 0;
}

void
UdpEchoServer2::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
UdpEchoServer2::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local6 = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
      m_socket6->Bind (local6);
      if (addressUtils::IsMulticast (local6))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket6);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, local6);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpEchoServer2::HandleRead, this));
  m_socket6->SetRecvCallback (MakeCallback (&UdpEchoServer2::HandleRead, this));
}

void
UdpEchoServer2::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_socket6 != 0)
    {
      m_socket6->Close ();
      m_socket6->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
UdpEchoServer2::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }

      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();

      NS_LOG_LOGIC ("Echoing packet");

      if (m_vmMigration == 0) // no VM migration
      {
          // create a packet
        X2Header x2Header;
        x2Header.SetMessageType (X2Header::IcnMessageSource);
        x2Header.SetProcedureCode (X2Header::HandoverPreparation);
        x2Header.SetLengthOfIes (1);
        x2Header.SetNumberOfIes (1);

        Ptr<Packet> packetPitToSource = Create<Packet> (14);
        packetPitToSource->AddHeader (x2Header);

        Ptr<Packet> packetPitToTarget = Create<Packet> (14);
        x2Header.SetMessageType (X2Header::IcnMessage);
        packetPitToTarget->AddHeader (x2Header);

        NS_LOG_INFO ("PIT Packet Size to Target eNB: " << packetPitToTarget->GetSize());
        NS_LOG_INFO ("PIT Packet Size to Source eNB: " << packetPitToSource->GetSize());

        uint16_t source_port = 4444;
        socket->SendTo (packetPitToSource, 0, InetSocketAddress (Ipv4Address ("11.0.0.1"), source_port));
        socket->SendTo (packetPitToTarget, 0, from);

        if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
        else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }
      }
      else
      {
          if (m_msgCounter == 0)
          {
                // create a packet
            X2Header x2Header;
            x2Header.SetMessageType (X2Header::IcnMessageSource);
            x2Header.SetProcedureCode (X2Header::HandoverPreparation);
            x2Header.SetLengthOfIes (1);
            x2Header.SetNumberOfIes (1);

            Ptr<Packet> packetPitToSource = Create<Packet> (14);
            packetPitToSource->AddHeader (x2Header);

            Ptr<Packet> migReqToTarget = Create<Packet> (2); // 9 bytes - 7 (header size)
            x2Header.SetMessageType (X2Header::MigrationRequest);
            migReqToTarget->AddHeader (x2Header);

            uint16_t source_port = 4444;
            socket->SendTo (packetPitToSource, 0, InetSocketAddress (Ipv4Address ("11.0.0.1"), source_port));
            socket->SendTo (migReqToTarget, 0, from);
          }
          else if (m_msgCounter == 1)
          {
            X2Header x2Header;
            x2Header.SetMessageType (X2Header::StartVmCmd);
            x2Header.SetProcedureCode (X2Header::HandoverPreparation);
            x2Header.SetLengthOfIes (1);
            x2Header.SetNumberOfIes (1);

            Ptr<Packet> startVm = Create<Packet> (6); // 13 Bytes - 7(header)
            startVm->AddHeader (x2Header);

            //uint16_t source_port = 4444;
            socket->SendTo (startVm, 0, from);
          }
          else if (m_msgCounter == 2)
          {
            X2Header x2Header;
            x2Header.SetMessageType (X2Header::IcnMessage);
            x2Header.SetProcedureCode (X2Header::HandoverPreparation);
            x2Header.SetLengthOfIes (1);
            x2Header.SetNumberOfIes (1);

            Ptr<Packet> packetPitToTarget = Create<Packet> (14);
            packetPitToTarget->AddHeader (x2Header);

            //uint16_t source_port = 4444;
            socket->SendTo (packetPitToTarget, 0, from);
          }

          m_msgCounter++;

          if (InetSocketAddress::IsMatchingType (from))
            {
                NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
            }
            else if (Inet6SocketAddress::IsMatchingType (from))
            {
                NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
            }

      }


    }
}

} // Namespace ns3
