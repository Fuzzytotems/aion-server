#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

/**
 * The AI of the flame vent levers in Idgel Dome ("flame_vent"): the use spawns a repelling flame cannon and two effects for 60 s on the
 * lever's side, and the lever dies.
 * <p>
 * Java: data/handlers/ai/instance/idgelDome/FlameVentAI.java, @AIName("flame_vent") (the marker is in the .cpp). C++: each despawn task
 * is pinned to the npc it deletes.
 *
 * @author Ritsu, Estrayl
 */
class FlameVentAI : public ActionItemNpcAI {
public:
	explicit FlameVentAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

private:
	/** C++: Java's repeated addTask(TaskId.DESPAWN, schedule(() -> npc.getController().delete(), 60, TimeUnit.SECONDS)) */
	static void despawnIn60s(Npc& npc);
};

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
