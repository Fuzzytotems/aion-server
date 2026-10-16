#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * An Ahserion assault pod ("ahserion_assault_pod"): 0.4 s after its spawn it casts Trooper Shock (20776), 4 s later three troopers 297191 land
 * around where it stood then, and 6 s later it is deleted.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAssaultPod.java, @AIName("ahserion_assault_pod") (the marker is in the
 * .cpp). C++: the tasks are pinned to this AI.
 *
 * @author Estrayl
 */
class AhserionAssaultPod : public GeneralNpcAI {
public:
	explicit AhserionAssaultPod(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void activate();

	void spawnDefenders(world::WorldPosition& p);
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
