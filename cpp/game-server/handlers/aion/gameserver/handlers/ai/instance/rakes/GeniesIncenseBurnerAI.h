#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of the genie's incense burner on the Steel Rake ("geniesincenseburner"): used, it targets itself and casts 18465.
 * <p>
 * Java: data/handlers/ai/instance/rakes/GeniesIncenseBurnerAI.java, @AIName("geniesincenseburner") (the marker is in the .cpp).
 *
 * @author xTz
 */
class GeniesIncenseBurnerAI : public ActionItemNpcAI {
public:
	explicit GeniesIncenseBurnerAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
