#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * An Ahserion sky assaulter ("ahserion_sky_assaulter"): 0.4 s after its spawn it drops an assault pod 297188 (gone after 6 s), casts Trooper
 * Shock (20776) 1 s later and 4 s later lands its troopers (by npc id 297352 or 297353) and is deleted.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionSkyAssaulterAI.java, @AIName("ahserion_sky_assaulter") (the marker is in
 * the .cpp). C++: the tasks are pinned to this AI, the pod's despawn task to the pod.
 *
 * @author Estrayl
 */
class AhserionSkyAssaulterAI : public GeneralNpcAI {
public:
	explicit AhserionSkyAssaulterAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void activate();

	void useSkill();

	void spawnDefenders();
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
