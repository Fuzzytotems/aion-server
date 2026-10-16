#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructAI.h"

#include <cstdint>
#include <map>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * A tank base controller of Ahserion's Flight ("ahserion_tank_controller"), by static id 180-183 (tanks C, B, D and A): under attack it tells
 * the map at most every 3 s; despawned or dead it deletes the tanks, flag and attackers within 40 m; dead it announces the break and, 3 s
 * later during the raid, spawns the base of the faction that dealt it the most damage (the Balaur when no team did more).
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionTankControllerAI.java, @AIName("ahserion_tank_controller") (the marker is in
 * the .cpp). C++: the shout reset and the base spawn are pinned to this AI.
 *
 * @author Yeats, Estrayl
 */
class AhserionTankControllerAI : public AhserionConstructAI {
private:
	runtime::AtomicBoolean canShout{AION_LOCK_CLASS(AhserionTankControllerAI::canShout), true}; // Java: = new AtomicBoolean(true)

public:
	explicit AhserionTankControllerAI(Npc& owner) : AhserionConstructAI(owner) {}

	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void broadcastAttack();

	void allowShout();

	void deleteRelatedNpcs();

protected:
	void handleDespawned() override;

public:
	void handleDied() override;

	/** Java: private; C++ public for the tests (a pure function of the damage map and the service's teams) */
	static services::panesterra::ahserion::PanesterraFaction findWinnerTeam(
		const std::map<services::panesterra::ahserion::PanesterraFaction, int32_t>& panesterraDamage);

private:
	void spawnNewTankBase(int32_t staticId, services::panesterra::ahserion::PanesterraFaction faction);
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
