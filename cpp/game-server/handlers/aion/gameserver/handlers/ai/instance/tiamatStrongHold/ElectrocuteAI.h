#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of the electrocution in Tiamat Stronghold ("electrocute"): it casts 20757 every 2 s from its spawn and is deleted after 10.5 s. No
 * decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/ElectrocuteAI.java, @AIName("electrocute") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class ElectrocuteAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit ElectrocuteAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void despawn();

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
