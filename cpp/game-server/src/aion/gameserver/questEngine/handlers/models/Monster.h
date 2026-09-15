#pragma once

#include "aion/gameserver/questEngine/handlers/models/Monster.xml.h"

#include <cstdint>
#include <vector>

namespace aion::gameserver::questEngine::handlers::models {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.models.Monster: one kill target of an XML quest.
 * <p>
 * C++ notes (P4-08): besides the bound templates, MonsterHuntData.register builds Monster objects with the setters and `addNpcIds`, so these are
 * not const. `npcIds` is nullopt for Java null.
 *
 * @author MrPoke, vlog, Bobobear, Artur, Pad
 */
class Monster : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/Monster.xml.inc"
public:
	/** appends the ids that are not in the list yet, in order (Java: creates the list if it is null) */
	void addNpcIds(const std::vector<int32_t>& value);
};

} // namespace aion::gameserver::questEngine::handlers::models
