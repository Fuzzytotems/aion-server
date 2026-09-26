#pragma once

#include <memory>

#include "aion/gameserver/configs/schedule/RiftSchedule.xml.h"

namespace aion::gameserver::configs::schedule {

/** Java com.aionemu.gameserver.configs.schedule.RiftSchedule. C++: load binds ./config/schedule/rift_schedule.xml like JAXBUtil.deserialize. @author
 * Source */
class RiftSchedule : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/schedule/RiftSchedule.xml.inc"
public:
	/** @throws commons::utils::Exception if the file cannot be read or bound */
	static std::unique_ptr<RiftSchedule> load();
};

} // namespace aion::gameserver::configs::schedule
