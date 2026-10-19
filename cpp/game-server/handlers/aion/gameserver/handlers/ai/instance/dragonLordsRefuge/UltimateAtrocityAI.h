#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the ultimate atrocities ("ultimate_atrocity"): fire, a quarter of their damage; from 0.5 s every 2 s their skill (by npc
 * id; none: they stay idle), gone after 11 s; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/UltimateAtrocityAI.java, @AIName("ultimate_atrocity") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class UltimateAtrocityAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit UltimateAtrocityAI(Npc& owner) : GeneralNpcAI(owner) {}

	ItemAttackType modifyAttackType(ItemAttackType type) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

protected:
	void handleSpawned() override;

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
