#include <SnoopAdapter>
#include <VDebugNew>

REGISTER_METACLASS(SnoopAdapter, SnoopCapture)

// ----------------------------------------------------------------------------
// SnoopAdapterIndex
// ----------------------------------------------------------------------------
SnoopAdapterIndex::SnoopAdapterIndex()
{
  m_adapterIndex = snoop::INVALID_ADAPTER_INDEX;
}

SnoopAdapterIndex::operator int()
{
  return m_adapterIndex;
}

void SnoopAdapterIndex::operator = (const int i)
{
  setAdapterIndex(i);
}

void SnoopAdapterIndex::setAdapterIndex(int value)
{
  if (m_adapterIndex == value) return;
  int maximum = SnoopInterfaces::instance().count() - 1;
  if (value > maximum )
  {
    LOG_ERROR("too big value(%d). maximum value is %d", value, maximum);
    return;
  }
  m_adapterIndex = value;
}

// ----------------------------------------------------------------------------
// SnoopAdapter
// ----------------------------------------------------------------------------
SnoopAdapter::SnoopAdapter(void* owner) : SnoopPcap(owner)
{
  // SnoopInterface::allInterfaces(); // for dependency // gilgil temp 2012.08.16
  adapterIndex = snoop::DEFAULT_ADAPTER_INDEX;
}

SnoopAdapter::~SnoopAdapter()
{
  close();
}

bool SnoopAdapter::doOpen()
{
  if (adapterIndex == snoop::INVALID_ADAPTER_INDEX)
  {
    SET_ERROR(SnoopError, "invalid adapter index(-1)", VERR_INVALID_INDEX);
    return false;
  }
  const SnoopInterface& intf = SnoopInterfaces::instance().at(adapterIndex);
  if (intf.dev == NULL)
  {
    SET_ERROR(SnoopError, "dev is NULL", VERR_OBJECT_IS_NULL);
    return false;
  }
  if (!pcapOpen((char*)qPrintable(intf.name), NULL, intf.dev))
  {
    return false;
  }

  return SnoopPcap::doOpen();
}

bool SnoopAdapter::doClose()
{
  return SnoopPcap::doClose();
}

void SnoopAdapter::load(VXml xml)
{
  SnoopPcap::load(xml);
  adapterIndex = xml.getInt("adapterIndex", adapterIndex);
}

void SnoopAdapter::save(VXml xml)
{
  SnoopPcap::save(xml);
  xml.setInt("adapterIndex", adapterIndex);
}

#ifdef QT_GUI_LIB
void SnoopAdapter::addOptionWidget(QLayout *layout)
{
  SnoopPcap::addOptionWidget(layout);
  QStringList strList;
  SnoopInterfaces& intfs = SnoopInterfaces::instance();
  int _count = intfs.count();
  for (int i = 0; i < _count; i++)
  {
    SnoopInterface& intf = (SnoopInterface&)intfs.at(i);
    QString value = intf.description;
    if (value == "") value = intf.name;
    strList.push_back(value);
  }
  VShowOption::addComboBox(layout, "cbxAdapterIndex", "Adapter", strList, adapterIndex);
}

void SnoopAdapter::saveOption(QDialog *dialog)
{
  SnoopPcap::saveOption(dialog);
  adapterIndex = dialog->findChild<QComboBox*>("cbxAdapterIndex")->currentIndex();
}
#endif // QT_GUI_LIB
