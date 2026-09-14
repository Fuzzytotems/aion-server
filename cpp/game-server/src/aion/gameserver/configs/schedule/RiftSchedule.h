#pragma once

#include "aion/gameserver/configs/schedule/RiftSchedule.xml.h"

namespace aion::gameserver::configs::schedule {

/** Java com.aionemu.gameserver.configs.schedule.RiftSchedule. @author Source */
class RiftSchedule : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/schedule/RiftSchedule.xml.inc"
public:
};

} // namespace aion::gameserver::configs::schedule
