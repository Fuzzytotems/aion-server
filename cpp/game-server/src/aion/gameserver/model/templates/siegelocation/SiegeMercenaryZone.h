#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/siegelocation/SiegeMercenaryZone.xml.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.SiegeMercenaryZone. @author Whoop */
class SiegeMercenaryZone : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/siegelocation/SiegeMercenaryZone.xml.inc"
public:
	/** Java: cooldown * 1000 (int arithmetic) */
	int32_t getCooldown() const { return static_cast<int32_t>(static_cast<uint32_t>(cooldown) * 1000u); }
};

} // namespace aion::gameserver::model::templates::siegelocation
