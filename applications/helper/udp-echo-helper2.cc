/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "udp-echo-helper2.h"
#include "ns3/udp-echo-server2.h"
#include "ns3/udp-echo-client.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

UdpEchoServerHelper2::UdpEchoServerHelper2 (uint16_t port)
{
  m_factory.SetTypeId (UdpEchoServer2::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
}

void
UdpEchoServerHelper2::SetAttribute (
  std::string name,
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
UdpEchoServerHelper2::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
UdpEchoServerHelper2::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
UdpEchoServerHelper2::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
UdpEchoServerHelper2::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<UdpEchoServer2> ();
  node->AddApplication (app);

  return app;
}

UdpEchoClientHelper2::UdpEchoClientHelper2 (Address address, uint16_t port)
{
  m_factory.SetTypeId (UdpEchoClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (address));
  SetAttribute ("RemotePort", UintegerValue (port));
}

UdpEchoClientHelper2::UdpEchoClientHelper2 (Ipv4Address address, uint16_t port)
{
  m_factory.SetTypeId (UdpEchoClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
}

UdpEchoClientHelper2::UdpEchoClientHelper2 (Ipv6Address address, uint16_t port)
{
  m_factory.SetTypeId (UdpEchoClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (Address(address)));
  SetAttribute ("RemotePort", UintegerValue (port));
}

void
UdpEchoClientHelper2::SetAttribute (
  std::string name,
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

void
UdpEchoClientHelper2::SetFill (Ptr<Application> app, std::string fill)
{
  app->GetObject<UdpEchoClient>()->SetFill (fill);
}

void
UdpEchoClientHelper2::SetFill (Ptr<Application> app, uint8_t fill, uint32_t dataLength)
{
  app->GetObject<UdpEchoClient>()->SetFill (fill, dataLength);
}

void
UdpEchoClientHelper2::SetFill (Ptr<Application> app, uint8_t *fill, uint32_t fillLength, uint32_t dataLength)
{
  app->GetObject<UdpEchoClient>()->SetFill (fill, fillLength, dataLength);
}

ApplicationContainer
UdpEchoClientHelper2::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
UdpEchoClientHelper2::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
UdpEchoClientHelper2::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
UdpEchoClientHelper2::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<UdpEchoClient> ();
  node->AddApplication (app);

  return app;
}

} // namespace ns3
