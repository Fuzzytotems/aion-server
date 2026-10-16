#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of the snare turrets ("eternal_bastion_snare_turret"): fire attacks; the first aggro pulls the creature (21116), then the turret vents
 * (21117), and pulls again only after the venting ends.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionSnareTurretAI.java, @AIName("eternal_bastion_snare_turret")
 * (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionSnareTurretAI : public EternalBastionAggressiveNpcAI {
private:
	runtime::AtomicBoolean isSkillDisabled{AION_LOCK_CLASS(EternalBastionSnareTurretAI::isSkillDisabled)}; // Java: = new AtomicBoolean()

public:
	explicit EternalBastionSnareTurretAI(Npc& owner) : EternalBastionAggressiveNpcAI(owner) {}

	model::templates::item::ItemAttackType modifyAttackType(model::templates::item::ItemAttackType type) override;

	void handleFinishAttack() override;

protected:
	void handleCreatureAggro(Creature& creature) override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEffectEnd(runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
