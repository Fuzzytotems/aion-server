#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Adjutant Anuhart's blade storm in Tiamat Stronghold ("bladestorm"): it casts 20748 every second from its spawn and is deleted after
 * 10 s. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/BladeStormAI.java, @AIName("bladestorm") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class BladeStormAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> spinTask{};

public:
	explicit BladeStormAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
