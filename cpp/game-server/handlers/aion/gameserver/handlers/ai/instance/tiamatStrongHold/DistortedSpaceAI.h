#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Terath's distorted space in Tiamat Stronghold ("distortedspace", 283096-283098): 283097 casts 20740 every 2 s from 0.5 s; 8 s
 * after the spawn the casts stop, 283097 casts 20742 and the npc dies, which deletes it. No decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/DistortedSpaceAI.java, @AIName("distortedspace") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class DistortedSpaceAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit DistortedSpaceAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void useskill();

	void cancelTask();

public:
	void handleDied() override;

	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
