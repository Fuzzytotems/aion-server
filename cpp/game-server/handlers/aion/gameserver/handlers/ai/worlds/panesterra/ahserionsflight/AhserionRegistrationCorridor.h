#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/services/panesterra/ahserion/fwd.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * The registration corridor of Ahserion's Flight ("ahserion_registration_corridor"), one per Panesterra faction by npc id: its spawn announces
 * STR_MSG_SVS_INVADE_DIRECT_PORTAL_OPEN to the map and it despawns after 10 minutes; a level-65 player who accepts STR_ASK_PASS_BY_SVS_DIRECT
 * _PORTAL joins its team at the start position while the raid runs and the team has room.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionRegistrationCorridor.java, @AIName("ahserion_registration_corridor") (the
 * marker is in the .cpp). C++: the despawn task is pinned to the owner (its controller holds it, TaskId.DESPAWN).
 *
 * @author Yeats, Estrayl
 */
class AhserionRegistrationCorridor : public GeneralNpcAI {
private:
	runtime::Field<std::optional<services::panesterra::ahserion::PanesterraFaction>> faction{}; // Java: null until handleSpawned

public:
	explicit AhserionRegistrationCorridor(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void handleDialogStart(Player& player) override;

	/** C++ only, for the tests: the faction the spawn chose (Java's private field) */
	std::optional<services::panesterra::ahserion::PanesterraFaction> getFaction() const { return faction.get(); }
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
