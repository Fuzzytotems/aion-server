#pragma once

#include "aion/gameserver/dataholders/RiftData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.RiftData. @author Source */
class RiftData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RiftData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
