#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Tahabata's fire storms in Tiamat Stronghold ("tahabatafirestorm"): it casts its storm every second from its spawn (20753 for the
 * last one, 283102, which stays; 20759 for the others, deleted after 20 s). No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/FireStormAI.java, @AIName("tahabatafirestorm") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class FireStormAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit FireStormAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void despawn();

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
