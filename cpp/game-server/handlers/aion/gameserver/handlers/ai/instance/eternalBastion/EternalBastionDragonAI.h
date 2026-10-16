#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's siege dragons ("eternal_bastion_dragon"): beyond 6 m they attack magically with 20 % more damage; immune to
 * abnormal states; their skill area comes with them.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionDragonAI.java, @AIName("eternal_bastion_dragon") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionDragonAI : public EternalBastionAggressiveNpcAI {
private:
	runtime::Field<double> dist{};

public:
	explicit EternalBastionDragonAI(Npc& owner) : EternalBastionAggressiveNpcAI(owner) {}

	model::animations::AttackTypeAnimation getAttackTypeAnimation(Creature& target) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	model::templates::item::ItemAttackType modifyAttackType(model::templates::item::ItemAttackType type) override;

	model::animations::AttackHandAnimation modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) override;

	bool ask(AIQuestion question) override;

protected:
	void handleSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
