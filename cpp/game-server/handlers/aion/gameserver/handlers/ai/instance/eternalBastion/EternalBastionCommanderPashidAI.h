#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Grand Commander Pashid ("eternal_bastion_commander_pashid"): it comes with its skill area and hates the fortress commander within 20
 * m, again two seconds after each change of target; beyond 10 m it attacks magically with 30 % more damage.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionCommanderPashidAI.java, @AIName("eternal_bastion_commander_pashid")
 * (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionCommanderPashidAI : public EternalBastionAggressiveNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> commander{};
	runtime::Field<double> dist{};

public:
	explicit EternalBastionCommanderPashidAI(Npc& owner) : EternalBastionAggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void hateCommander(int32_t hate);

protected:
	/** Simulate retail behaviour here: Pashid and the defense commander occasionally exchange messages, with shouts and mutual hate */
	void handleTargetChanged(Creature& creature) override;

public:
	model::animations::AttackTypeAnimation getAttackTypeAnimation(Creature& target) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	model::templates::item::ItemAttackType modifyAttackType(model::templates::item::ItemAttackType type) override;

	model::animations::AttackHandAnimation modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
