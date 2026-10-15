#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of Raksha's sealing wall ("raksha_sealing_wall"): it does not think; the first player within 35 m calls Raksha (the stronger one
 * unless Sharik, the Flamelord and the seal guard are all dead) and the wall goes.
 * <p>
 * Java: data/handlers/ai/instance/raksang/RakshaSealingWallAI.java, @AIName("raksha_sealing_wall") (the marker is in the .cpp).
 *
 * @author xTz
 */
class RakshaSealingWallAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean startedEvent{AION_LOCK_CLASS(RakshaSealingWallAI::startedEvent), false}; // Java: = new AtomicBoolean(false)

public:
	explicit RakshaSealingWallAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleCreatureMoved(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
