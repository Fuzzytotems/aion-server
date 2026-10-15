#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the fallen Reians in Rentus Base ("fallen_reian"): the first player within 20 m opens the Reian's door (16 or 54).
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/FallenReianAI.java, @AIName("fallen_reian") (the marker is in the .cpp).
 *
 * @author xTz
 */
class FallenReianAI : public NpcAI {
private:
	runtime::AtomicBoolean isCollapsed{AION_LOCK_CLASS(FallenReianAI::isCollapsed), false}; // Java: = new AtomicBoolean(false)

public:
	explicit FallenReianAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleCreatureMoved(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
