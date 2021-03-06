// ----------------------------------------------------------------------------
//
// Snoop Component Suite version 8.0
//
// http://www.gilgil.net
//
// Copyright (c) Gilbert Lee All rights reserved
//
// ----------------------------------------------------------------------------

#ifndef __SNOOP_CAPTURE_H__
#define __SNOOP_CAPTURE_H__

#include <Snoop>
//#include <VMetaClass> // gilgil temp 2012.08.26
#include <VThread>
#include <VObjectWidget>

// ----------------------------------------------------------------------------
// SnoopCapture
// ----------------------------------------------------------------------------
/// Base class of all capture classes
class SnoopCapture : public VObject, protected VRunnable, public VShowOption
{
  Q_OBJECT

public:
  SnoopCapture(void* owner = NULL);
  virtual ~SnoopCapture();

protected:
  virtual bool doOpen();
  virtual bool doClose();

public:
  virtual int read(SnoopPacket* packet);
  virtual int write(SnoopPacket* packet);
  virtual int write(u_char* buf, int size, WINDIVERT_ADDRESS* divertAddr = NULL);

public:
  virtual SnoopCaptureType captureType() { return SnoopCaptureType::None; }
  virtual int              dataLink()    { return DLT_NULL; }
  virtual int              relay(SnoopPacket* packet);

  //
  // Properties
  //
public:
  bool         autoRead;

protected:
  virtual void run();

signals:
  void captured(SnoopPacket* packet);

protected:
  SnoopPacket packet;

public:
  virtual void load(VXml xml);
  virtual void save(VXml xml);

#ifdef QT_GUI_LIB
public: // for VShowOption
  virtual void addOptionWidget(QLayout* layout);
  virtual void saveOption(QDialog* dialog);
#endif // QT_GUI_LIB
};

#endif // __SNOOP_CAPTURE_H__
