#ifndef X2_HEADER_H
#define X2_HEADER_H

#include "ns3/header.h"

#include <vector>

namespace ns3 {


class X2Header : public Header
{
public:
  X2Header ();
  virtual ~X2Header ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;


  uint8_t GetMessageType () const;
  void SetMessageType (uint8_t messageType);

  uint8_t GetProcedureCode () const;
  void SetProcedureCode (uint8_t procedureCode);

  void SetLengthOfIes (uint32_t lengthOfIes);
  void SetNumberOfIes (uint32_t numberOfIes);


  enum ProcedureCode_t {
    HandoverPreparation     = 0,
    LoadIndication          = 2,
    SnStatusTransfer        = 4,
    UeContextRelease        = 5,
    ResourceStatusReporting = 10
  };

  enum TypeOfMessage_t {
    InitiatingMessage       = 0,
    SuccessfulOutcome       = 1,
    UnsuccessfulOutcome     = 2,
    IcnMessage              = 3,   // new
    IcnMessageSource        = 4,   // new
    MigrationRequest        = 5, // new
    StartVmCmd              = 6
  };

private:
  uint32_t m_headerLength;

  uint8_t m_messageType;
  uint8_t m_procedureCode;

  uint32_t m_lengthOfIes;
  uint32_t m_numberOfIes;
};

}

#endif
