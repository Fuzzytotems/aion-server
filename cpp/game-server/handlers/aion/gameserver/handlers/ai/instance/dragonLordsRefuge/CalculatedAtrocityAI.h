#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the calculated atrocities ("calculated_atrocity"): it never thinks; fire, no damage taken, 70 % of its damage; from 0.5 s
 * every 2 s 21894 on every player it sees within 45 m in front of it; gone after 11 s; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/CalculatedAtrocityAI.java, @AIName("calculated_atrocity") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class CalculatedAtrocityAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit CalculatedAtrocityAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

	ItemAttackType modifyAttackType(ItemAttackType type) override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

protected:
	void handleSpawned() override;

private:
	void calculateAndApplyDamage();

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
