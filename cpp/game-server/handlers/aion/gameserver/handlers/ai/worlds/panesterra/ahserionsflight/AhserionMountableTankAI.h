#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * A mountable tank of Ahserion's Flight ("ahserion_mountable_tank"): owned by a faction by npc id, it despawns after 12 minutes; during the raid
 * the first finished use by a member of its faction's team boards the player (a chariot or an Ignus Engine skill, level 65) and deletes the
 * tank; no AP, XP, DP or loot.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionMountableTankAI.java, @AIName("ahserion_mountable_tank") (the marker is in the
 * .cpp). C++: the despawn task is pinned to the owner (its controller holds it, TaskId.DESPAWN).
 *
 * @author Estrayl
 */
class AhserionMountableTankAI : public ActionItemNpcAI {
private:
	runtime::AtomicBoolean canUse{AION_LOCK_CLASS(AhserionMountableTankAI::canUse), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<services::panesterra::ahserion::PanesterraFaction> ownerFaction{};

public:
	explicit AhserionMountableTankAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleUseItemFinish(Player& player) override;

private:
	bool canUseTank(Player& player);

	int32_t getSkillId();

public:
	bool ask(AIQuestion question) override;

	/** C++ only, for the tests: the faction the spawn chose (Java's private field) */
	services::panesterra::ahserion::PanesterraFaction getOwnerFaction() const { return ownerFaction.get(); }
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
