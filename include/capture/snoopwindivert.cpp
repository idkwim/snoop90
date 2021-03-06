#include <SnoopWinDivert>
#include <VDebugNew>

REGISTER_METACLASS(SnoopWinDivert, SnoopCapture)

// ----------------------------------------------------------------------------
// SnoopWinDivertLib
// ----------------------------------------------------------------------------
class SnoopWinDivertLib
{
protected:
  QLibrary* lib;

public:
  typedef HANDLE (*WinDivertOpenFunc)(
      __in        const char *filter,
      __in        DIVERT_LAYER layer,
      __in        INT16 priority,
      __in        UINT64 flags);
  WinDivertOpenFunc WinDivertOpen;

  typedef BOOL (*WinDivertCloseFunc)(
      __in        HANDLE handle);
  WinDivertCloseFunc WinDivertClose;

  typedef BOOL (*WinDivertSetParamFunc)(
      __in        HANDLE handle,
      __in        DIVERT_PARAM param,
      __in        UINT64 value);
  WinDivertSetParamFunc WinDivertSetParam;

  typedef BOOL (*WinDivertRecvFunc)(
      __in        HANDLE handle,
      __out       PVOID pPacket,
      __in        UINT packetLen,
      __out_opt   PDIVERT_ADDRESS pAddr,
      __out_opt   UINT *readLen);
  WinDivertRecvFunc WinDivertRecv;

  typedef BOOL (*WinDivertSendFunc)(
      __in        HANDLE handle,
      __in        PVOID pPacket,
      __in        UINT packetLen,
      __in        PDIVERT_ADDRESS pAddr,
      __out_opt   UINT *writeLen);
  WinDivertSendFunc WinDivertSend;

public:
  bool ok;

private: // singleton
  SnoopWinDivertLib()
  {
    WinDivertOpen     = NULL;
    WinDivertClose    = NULL;
    WinDivertSetParam = NULL;
    WinDivertRecv     = NULL;
    WinDivertSend     = NULL;
    ok             = false;

    lib = new QLibrary("WinDivert.dll");
    if (!lib->load())
    {
      LOG_FATAL("lib->load() return false");
      return; // gilgil temp 2013.11.29
    }

    if ((WinDivertOpen     = (WinDivertOpenFunc)    lib->resolve("WinDivertOpen"))     == NULL) LOG_FATAL("can not find WinDivertOpen");
    if ((WinDivertClose    = (WinDivertCloseFunc)   lib->resolve("WinDivertClose"))    == NULL) LOG_FATAL("can not find WinDivertClose");
    if ((WinDivertSetParam = (WinDivertSetParamFunc)lib->resolve("WinDivertSetParam")) == NULL) LOG_FATAL("can not find WinDivertSetParam");
    if ((WinDivertRecv     = (WinDivertRecvFunc)    lib->resolve("WinDivertRecv"))     == NULL) LOG_FATAL("can not find WinDivertRecv");
    if ((WinDivertSend     = (WinDivertSendFunc)    lib->resolve("WinDivertSend"))     == NULL) LOG_FATAL("can not find WinDivertSend");

    ok = true;
  }

  virtual ~SnoopWinDivertLib()
  {
    lib->unload();
    SAFE_DELETE(lib);
  }

public:
  static SnoopWinDivertLib& instance()
  {
    static SnoopWinDivertLib windivertLib;
    return windivertLib;
  }
};

// ----------------------------------------------------------------------------
// SnoopWinDivert
// ----------------------------------------------------------------------------
SnoopWinDivert::SnoopWinDivert(void* owner) : SnoopCapture(owner)
{
  filter              = "true";
  priority            = 0;
  layer               = WINDIVERT_LAYER_NETWORK;
  flags               = 0; //WINDIVERT_FLAG_SNIFF; // gilgil temp 2013.12.06
  queueLen            = 8192;
  queueTime           = 1024;
  autoCorrectChecksum = true;

  handle    = 0;
}

SnoopWinDivert::~SnoopWinDivert()
{
  close();
}

bool SnoopWinDivert::doOpen()
{
  SnoopWinDivertLib& lib = SnoopWinDivertLib::instance();
  if (!lib.ok)
  {
    SET_ERROR(WinDivertError, "can not load WinDivert", VERR_LOAD_FAIL);
    return false;
  }

  handle = lib.WinDivertOpen(qPrintable(filter), layer, priority, flags);
  if (handle == INVALID_HANDLE_VALUE)
  {
    DWORD lastError = GetLastError();

    QString msg;
    switch (lastError)
    {
      case ERROR_INVALID_PARAMETER:  msg = "ERROR_INVALID_PARAMETER";     break;
      case ERROR_FILE_NOT_FOUND:     msg = "ERROR_FILE_NOT_FOUND";        break;
      case ERROR_ACCESS_DENIED:      msg = "ERROR_ACCESS_DENIED";         break;
      case ERROR_INVALID_IMAGE_HASH: msg = "ERROR_INVALID_IMAGE_HASH";    break;
      default:                       msg = qformat("unknown error %u", lastError); break;
    }
    SET_ERROR(WinDivertError, qformat("error in WinDivertOpen %s", qPrintable(msg)), lastError);
    return false;
  }

  if (!lib.WinDivertSetParam(handle, WINDIVERT_PARAM_QUEUE_LEN, queueLen))
  {
    DWORD lastError = GetLastError();
    SET_ERROR(WinDivertError, "error in DivertSetParam(DIVERT_PARAM_QUEUE_LEN)" , lastError);
    return false;
  }
  if (!lib.WinDivertSetParam(handle, WINDIVERT_PARAM_QUEUE_TIME, queueTime))
  {
    DWORD lastError = GetLastError();
    SET_ERROR(WinDivertError, "error in DivertSetParam(DIVERT_PARAM_QUEUE_TIME)" , lastError);
    return false;
  }

  if (!SnoopCapture::doOpen()) return false;

  return true;
}

bool SnoopWinDivert::doClose()
{
  SnoopWinDivertLib& lib = SnoopWinDivertLib::instance();
  if (!lib.ok)
  {
    SET_ERROR(WinDivertError, "can not load WinDivert", VERR_LOAD_FAIL);
    return false;
  }

  if (!lib.WinDivertClose(handle))
  {
    DWORD lastError = GetLastError();
    LOG_ERROR("WinDivertClose return FALSE last error=%d(0x%x)", lastError, lastError);
  }
  handle = 0;

  bool res = SnoopCapture::doClose();
  return res;
}

int SnoopWinDivert::read(SnoopPacket* packet)
{
  SnoopWinDivertLib& lib = SnoopWinDivertLib::instance();
  if (!lib.ok)
  {
    SET_ERROR(WinDivertError, "can not load WinDivert", VERR_LOAD_FAIL);
    return VERR_FAIL;
  }

  packet->clear();
  UINT readLen;
  BOOL res = lib.WinDivertRecv(handle, this->pktData + sizeof(ETH_HDR), MAXBUF - sizeof(ETH_HDR), &packet->divertAddr, &readLen);
  if (!res)
  {
    DWORD lastError = GetLastError();
    SET_DEBUG_ERROR(WinDivertError, qformat("WinDivertRecv return FALSE last error=%d(0x%x)", lastError, lastError), lastError);
    return VERR_FAIL;
  }
  readLen += sizeof(ETH_HDR);

  // LOG_DEBUG("ifIdx=%u subIfIdx=%u Direction=%u readLen=%u", packet->divertAddr.IfIdx, packet->divertAddr.SubIfIdx, packet->divertAddr.Direction, readLen); // gilgil temp 2013.12.05

  ETH_HDR* ethHdr = (ETH_HDR*)pktData;
  ethHdr->ether_dhost = Mac::cleanMac();
  ethHdr->ether_shost = Mac::cleanMac();
  ethHdr->ether_type  = htons(ETHERTYPE_IP);

  this->pktHdr.caplen = readLen;
  this->pktHdr.len    = readLen;

  packet->pktData = this->pktData;
  packet->pktHdr  = &this->pktHdr;
  packet->ethHdr  = ethHdr;

  if (autoCorrectChecksum)
  {
    if (SnoopEth::isIp(packet->ethHdr, &packet->ipHdr))
    {
      if (SnoopIp::isTcp(packet->ipHdr, &packet->tcpHdr))
        packet->tcpHdr->th_sum = htons(SnoopTcp::checksum(packet->ipHdr, packet->tcpHdr));
      else if (SnoopIp::isUdp(packet->ipHdr, &packet->udpHdr))
        packet->udpHdr->uh_sum = htons(SnoopUdp::checksum(packet->ipHdr, packet->udpHdr));
      packet->ipHdr->ip_sum  = htons(SnoopIp::checksum(packet->ipHdr));
    }
  }

  return (int)readLen;
}

int SnoopWinDivert::write(SnoopPacket* packet)
{
  return write(packet->pktData, packet->pktHdr->caplen, &packet->divertAddr);
}

int SnoopWinDivert::write(u_char* buf, int size, WINDIVERT_ADDRESS* divertAddr)
{
  SnoopWinDivertLib& lib = SnoopWinDivertLib::instance();
  if (!lib.ok)
  {
    SET_ERROR(WinDivertError, "can not load WinDivert", VERR_LOAD_FAIL);
    return false;
  }

  UINT writeLen;
  BOOL res = lib.WinDivertSend(handle, buf + sizeof(ETH_HDR), (UINT)size - sizeof(ETH_HDR), divertAddr, &writeLen);
  if (!res)
  {
    DWORD lastError = GetLastError();
    SET_DEBUG_ERROR(WinDivertError, qformat("WinDivertSend return FALSE last error=%d(0x%x)", lastError, lastError), lastError);
    return VERR_FAIL;
  }
  return (int)writeLen;
}

SnoopCaptureType SnoopWinDivert::captureType()
{
  if (flags & DIVERT_FLAG_SNIFF) return SnoopCaptureType::OutOfPath;
  return SnoopCaptureType::InPath;
}

int SnoopWinDivert::relay(SnoopPacket* packet)
{
  //
  // Checksum
  //
  if (packet->tcpChanged) packet->tcpHdr->th_sum = htons(SnoopTcp::checksum(packet->ipHdr, packet->tcpHdr));
  if (packet->udpChanged) packet->udpHdr->uh_sum = htons(SnoopUdp::checksum(packet->ipHdr, packet->udpHdr));
  if (packet->ipChanged)  packet->ipHdr->ip_sum  = htons(SnoopIp::checksum(packet->ipHdr));

  return write(packet);
}

void SnoopWinDivert::load(VXml xml)
{
  SnoopCapture::load(xml);

  filter              = xml.getStr("filter", filter);
  priority            = (UINT16)xml.getInt("priority", (int)priority);
  layer               = (DIVERT_LAYER)xml.getInt("layer", (int)layer);
  flags               = (UINT64)xml.getInt("flags", (int)flags);
  queueLen            = (UINT64)xml.getInt("queueLen", (int)queueLen);
  queueTime           = (UINT64)xml.getInt("queueTime", (int)queueTime);
  autoCorrectChecksum = xml.getBool("autoCorrectChecksum", autoCorrectChecksum);
}

void SnoopWinDivert::save(VXml xml)
{
  SnoopCapture::save(xml);

  xml.setStr("filter", filter);
  xml.setInt("priority", (int)priority);
  xml.setInt("layer", (int)layer);
  xml.setInt("flags", (int)flags);
  xml.setInt("queueLen", (int)queueLen);
  xml.setInt("queueTime", (int)queueTime);
  xml.setBool("autoCorrectChecksum", autoCorrectChecksum);
}

#ifdef QT_GUI_LIB
void SnoopWinDivert::addOptionWidget(QLayout* layout)
{
  SnoopCapture::addOptionWidget(layout);
  VShowOption::addLineEdit(layout, "leFilter",               "Filter",                filter);
  VShowOption::addLineEdit(layout, "lePriority",             "Priority",              QString::number(priority));
  VShowOption::addLineEdit(layout, "leLayer",                "Layer",                 QString::number(layer));
  VShowOption::addCheckBox(layout, "chkFlagSniff",           "Flag Sniff(OutOfPath)", flags & WINDIVERT_FLAG_SNIFF);
  VShowOption::addCheckBox(layout, "chkFlagDrop",            "Flag Drop",             flags & WINDIVERT_FLAG_DROP);
  VShowOption::addCheckBox(layout, "chkFlagNoChecksum",      "Flag No Checksum",      flags & WINDIVERT_FLAG_NO_CHECKSUM);
  VShowOption::addLineEdit(layout, "leQueueLen",             "Queue Len",             QString::number(queueLen));
  VShowOption::addLineEdit(layout, "leQueueTime",            "Queue Time",            QString::number(queueTime));
  VShowOption::addCheckBox(layout, "chkAutoCorrectChecksum", "Auto Correct Checksum", autoCorrectChecksum);
}

void SnoopWinDivert::saveOption(QDialog* dialog)
{
  SnoopCapture::saveOption(dialog);
  filter    = dialog->findChild<QLineEdit*>("leFilter")->text();
  priority  = dialog->findChild<QLineEdit*>("lePriority")->text().toUShort();
  layer     = (DIVERT_LAYER) dialog->findChild<QLineEdit*>("leLayer")->text().toInt();
  flags     = 0;

  if (dialog->findChild<QCheckBox*>("chkFlagSniff")->checkState()      == Qt::Checked) flags |= WINDIVERT_FLAG_SNIFF;
  if (dialog->findChild<QCheckBox*>("chkFlagDrop")->checkState()       == Qt::Checked) flags |= WINDIVERT_FLAG_DROP;
  if (dialog->findChild<QCheckBox*>("chkFlagNoChecksum")->checkState() == Qt::Checked) flags |= WINDIVERT_FLAG_NO_CHECKSUM;
  queueLen  = dialog->findChild<QLineEdit*>("leQueueLen")->text().toULongLong();
  queueTime = dialog->findChild<QLineEdit*>("leQueueTime")->text().toULongLong();
  autoCorrectChecksum = dialog->findChild<QCheckBox*>("chkAutoCorrectChecksum")->checkState() == Qt::Checked;
}
#endif // QT_GUI_LIB
