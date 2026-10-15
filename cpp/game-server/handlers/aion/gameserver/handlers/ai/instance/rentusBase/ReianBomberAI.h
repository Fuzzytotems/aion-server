#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the Reian bombers in Rentus Base ("reian_bomber"): spawned they walk to the boss; at step 7 (and after each later walk) they
 * stop, cast 19374 and 8 s later help at the next of three places (the crystal there goes, a healer comes unless one stands) and walk on.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/ReianBomberAI.java, @AIName("reian_bomber") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ReianBomberAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean hasArrivedBoss{AION_LOCK_CLASS(ReianBomberAI::hasArrivedBoss), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<int32_t> position{1};

public:
	explicit ReianBomberAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

private:
	void startHelpEvent();

	void deleteNpc(runtime::Ptr<Npc> npc);

	void help(float x, float y, float z);
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
