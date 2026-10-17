#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/services/panesterra/ahserion/fwd.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {

/**
 * An advance corridor to Ahserion's Flight ("panesterra_advance_corridor"): it despawns after despawnInMin minutes (10); an officer of at least
 * one star and level 65 is asked STR_ASK_PASS_BY_SVS_DIRECT_PORTAL and, while his race's team has room, joins it at its start position (the
 * Elyos Ivy Temple, the Asmodians Alpine Temple, hard-coded as in Java).
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/AdvanceCorridorAI.java, @AIName("panesterra_advance_corridor") (the marker is in the .cpp). C++:
 * the despawn task is pinned to the owner (its controller holds it, TaskId.DESPAWN).
 *
 * @author Estrayl
 */
class AdvanceCorridorAI : public GeneralNpcAI {
protected:
	runtime::Field<int32_t> despawnInMin{10}; // Java: protected int despawnInMin = 10

public:
	explicit AdvanceCorridorAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void handleDialogStart(Player& player) override;

private:
	/** Hard-coded for now. TODO: Faction selection should be moved into a dedicated matchmaking service */
	services::panesterra::ahserion::PanesterraFaction getFactionToAssign(Player& player);
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
