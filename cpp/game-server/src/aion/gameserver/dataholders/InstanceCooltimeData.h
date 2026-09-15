#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/InstanceCooltimeData.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.InstanceCooltimeData. @author VladimirZ */
class InstanceCooltimeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/InstanceCooltimeData.xml.inc"
public:
	/** Java: NullPointerException if the world has no cooltime template */
	int32_t getInstanceMaxCountByWorldId(int32_t worldId) const;

	int64_t calculateInstanceEntranceCooltime(model::gameobjects::player::Player& player, int32_t worldId) const;
};

} // namespace aion::gameserver::dataholders
