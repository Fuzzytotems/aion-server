#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.xml.h"

namespace aion::gameserver::model::templates::teleport {

/** Java com.aionemu.gameserver.model.templates.teleport.TeleporterTemplate. @author orz */
class TeleporterTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.xml.inc"
public:
	/** @throws NullPointerException (Java) without npc_ids */
	bool containNpc(int32_t npcId) const;
};

} // namespace aion::gameserver::model::templates::teleport
