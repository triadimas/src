/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

#ifndef EPC_X2_H
#define EPC_X2_H

#include "ns3/socket.h"
#include "ns3/callback.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/epc-x2-sap.h"
#include "ns3/epc-enb-application.h" // edit
#include <iostream> // edit
#include <fstream>  // edit

#include <map>

using namespace std;

namespace ns3 {


class X2IfaceInfo : public SimpleRefCount<X2IfaceInfo>
{
public:
  X2IfaceInfo (Ipv4Address remoteIpAddr, Ptr<Socket> localCtrlPlaneSocket, Ptr<Socket> localUserPlaneSocket);
  virtual ~X2IfaceInfo (void);

  X2IfaceInfo& operator= (const X2IfaceInfo &);

public:
  Ipv4Address   m_remoteIpAddr;
  Ptr<Socket>   m_localCtrlPlaneSocket;
  Ptr<Socket>   m_localUserPlaneSocket;
};


class X2CellInfo : public SimpleRefCount<X2CellInfo>
{
public:
  X2CellInfo (uint16_t localCellId, uint16_t remoteCellId);
  virtual ~X2CellInfo (void);

  X2CellInfo& operator= (const X2CellInfo &);

public:
  uint16_t m_localCellId;
  uint16_t m_remoteCellId;
};


/**
 * \ingroup lte
 *
 * This entity is installed inside an eNB and provides the functionality for the X2 interface
 */
class EpcX2 : public Object
{
  friend class EpcX2SpecificEpcX2SapProvider<EpcX2>;

public:
  /**
   * Constructor
   */
  EpcX2 ();

  EpcX2(const EpcX2&);

  /**
   * Destructor
   */
  virtual ~EpcX2 (void);

  static TypeId GetTypeId (void);
  virtual void DoDispose (void);


  //void GetEpcEnbApplication (Ptr<EpcEnbApplication> enb);

  /**
   * \param s the X2 SAP User to be used by this EPC X2 entity
   */
  void SetEpcX2SapUser (EpcX2SapUser * s);

  /**
   * \param s the X2 SAP Provider interface offered by this EPC X2 entity
   */
  EpcX2SapProvider* GetEpcX2SapProvider ();


  /**
   * \param s the X2 SAP Provider interface offered by this EPC X2 entity
   */
  void AddX2Interface (uint16_t enb1CellId, Ipv4Address enb1X2Address, uint16_t enb2CellId, Ipv4Address enb2X2Address);


  /**
   * Method to be assigned to the recv callback of the X2-C (X2 Control Plane) socket.
   * It is called when the eNB receives a packet from the peer eNB of the X2-C interface
   *
   * \param socket socket of the X2-C interface
   */
  void RecvFromX2cSocket (Ptr<Socket> socket);

  /**
   * Method to be assigned to the recv callback of the X2-U (X2 User Plane) socket.
   * It is called when the eNB receives a packet from the peer eNB of the X2-U interface
   *
   * \param socket socket of the X2-U interface
   */
  void RecvFromX2uSocket (Ptr<Socket> socket);

  void GetEpcEnbApplication (Ptr<EpcEnbApplication> enb); // new

  void SendVm (); // new

protected:
  // Interface provided by EpcX2SapProvider
  virtual void DoSendHandoverRequest (EpcX2SapProvider::HandoverRequestParams params);
  virtual void DoSendHandoverRequestAck (EpcX2SapProvider::HandoverRequestAckParams params);
  virtual void DoSendHandoverPreparationFailure (EpcX2SapProvider::HandoverPreparationFailureParams params);
  virtual void DoSendSnStatusTransfer (EpcX2SapProvider::SnStatusTransferParams params);
  virtual void DoSendUeContextRelease (EpcX2SapProvider::UeContextReleaseParams params);
  virtual void DoSendLoadInformation (EpcX2SapProvider::LoadInformationParams params);
  virtual void DoSendResourceStatusUpdate (EpcX2SapProvider::ResourceStatusUpdateParams params);
  virtual void DoSendUeData (EpcX2SapProvider::UeDataParams params);
  virtual void DoSendUeData2 (EpcX2SapProvider::UeDataParams params); // new

  EpcX2SapUser* m_x2SapUser;
  EpcX2SapProvider* m_x2SapProvider;


private:

   Ptr<EpcEnbApplication> epcEnbApp;

  /**
   * Map the targetCellId to the corresponding (sourceSocket, remoteIpAddr) to be used
   * to send the X2 message
   */
  std::map < uint16_t, Ptr<X2IfaceInfo> > m_x2InterfaceSockets;

  /**
   * Map the localSocket (the one receiving the X2 message)
   * to the corresponding (sourceCellId, targetCellId) associated with the X2 interface
   */
  std::map < Ptr<Socket>, Ptr<X2CellInfo> > m_x2InterfaceCellIds;

  /**
   * UDP ports to be used for the X2 interfaces: X2-C and X2-U
   */
  uint16_t m_x2cUdpPort;
  uint16_t m_x2uUdpPort;

  std::vector< Ptr<Socket> > m_socket; // new

  struct EpcUeDataParams
  {
    uint16_t    sourceCellId;
    uint16_t    targetCellId;
    uint32_t    gtpTeid;
  };

  std::vector<EpcX2::EpcUeDataParams> m_params; // new
  std::vector< Ptr<Packet> > m_ueData; // new

  std::vector<EpcX2::EpcUeDataParams> m_paramsRecv; // new
  std::vector< Ptr<Packet> > m_ueDataRecv; // new

  std::vector<bool> m_sendUeData; // new
  std::vector<bool> m_icnMsgSource; // new

  std::vector<Ipv4Address> m_ipBuff;

  Ipv4Address m_ipv4Address;
  Ipv4Address m_icnIpSource;

  std::string m_simName; // edit

  ostringstream x2FileDataSent; // edit
  ostringstream x2FileDataRecv; // edit
  ostringstream ccMsgFileSent; // edit
  ostringstream ccMsgFileRecvSrc; // edit
  ostringstream ccMsgFileRecvTrg; // edit
  ostringstream ackFile; // edit
  ostringstream vmFileSent; // edit
  ostringstream vmFileRecv; // edit

  uint32_t m_counter; // edit
  uint32_t m_numOfPacket; // edit
  uint32_t m_vmMigration; // edit
  uint32_t m_targetCounter; // edit

};

} //namespace ns3

#endif // EPC_X2_H
