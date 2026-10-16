#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the dancing flames in Rentus Base ("dancing_flame"): no damage; every 9 s from 10 s it buffs while a player it knows is within
 * 10 m; no respawn, loot or decay.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/DancingFlameAI.java, @AIName("dancing_flame") (the marker is in the .cpp).
 *
 * @author xTz, Estrayl
 */
class DancingFlameAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> buffTask{};

public:
	explicit DancingFlameAI(Npc& owner) : GeneralNpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

private:
	void startBuffTask();

protected:
	void handleSpawned() override;

	void handleDespawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
