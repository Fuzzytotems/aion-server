#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Terath's gravity in Tiamat Stronghold ("gravity"): it casts 20738 every 3.25 s from its spawn and is deleted after 20 s. No decay,
 * respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/GravityAI.java, @AIName("gravity") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Luzien
 */
class GravityAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit GravityAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
