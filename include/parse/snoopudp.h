// ----------------------------------------------------------------------------
//
// Snoop Component Suite version 8.0
//
// http://www.gilgil.net
//
// Copyright (c) Gilbert Lee All rights reserved
//
// ----------------------------------------------------------------------------

#ifndef __SNOOP_UDP_H__
#define __SNOOP_UDP_H__

#include <SnoopIp>

// ----------------------------------------------------------------------------
// SnoopUdp
// ----------------------------------------------------------------------------
class SnoopUdp
{
public:
  static bool   isData(IP_HDR* ipHdr, UDP_HDR* udpHdr, char** udpData = NULL, int* udpDataLen = NULL);
  static UINT16 checksum(IP_HDR* ipHdr, UDP_HDR* udpHdr);

public:
  static bool parseAll(SnoopPacket* packet);
};

#endif // __SNOOP_UDP_H__
