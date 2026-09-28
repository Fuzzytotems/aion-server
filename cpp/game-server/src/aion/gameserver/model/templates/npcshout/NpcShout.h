#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/npcshout/NpcShout.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/** Java com.aionemu.gameserver.model.templates.npcshout.NpcShout. @author Rolandas */
class NpcShout : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/NpcShout.xml.inc"
public:
	/** @return 0 without skill_no */
	int32_t getSkillNo() const { return skillNo.value_or(0); }

	/** @return 0 without poll_delay */
	int32_t getPollDelay() const { return pollDelay.value_or(0); }

	int32_t getShoutRange(gameobjects::Npc& npc) const;
};

} // namespace aion::gameserver::model::templates::npcshout
