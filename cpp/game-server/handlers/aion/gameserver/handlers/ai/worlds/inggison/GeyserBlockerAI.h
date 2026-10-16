#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

/**
 * The AI of the mobs blocking Inggison's geysers ("geyserblocker"): a geyser (700545) it sees within 5 m is deleted and its static id kept;
 * the mob's death spawns the geyser again with that static id at the mob's spawn point and tells the players within 30 m
 * (STR_MSG_WINDBOX_TRIGGER_ON_INFO).
 * <p>
 * Java: data/handlers/ai/worlds/inggison/GeyserBlockerAI.java, @AIName("geyserblocker") (the marker is in the .cpp).
 *
 * @author Neon
 */
class GeyserBlockerAI : public AggressiveNpcAI {
private:
	runtime::Field<int32_t> staticId{-1}; // Java: = -1

public:
	explicit GeyserBlockerAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDied() override;

	void handleCreatureSee(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::inggison
