#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a servant ("servant": the Priest's Divine Mirror, the Gladiator's Battle Banner, the Spiritmaster's Taunting Spirit, ...): it does
 * not think or move; 200 ms after its spawn it targets its creator's target (a totem targets itself) and casts a random skill of its list
 * every few seconds until the target is gone or dead.
 * <p>
 * Java: data/handlers/ai/ServantNpcAI.java, @AIName("servant") (the marker is in the .cpp).
 */
class ServantNpcAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit ServantNpcAI(Npc& owner) : GeneralNpcAI(owner) {}

	void think() override;

	bool canThink() override;

protected:
	void handleSpawned() override;

private:
	void healOrAttack();

public:
	bool isMoveSupported() override;

private:
	void cancelTask();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai
