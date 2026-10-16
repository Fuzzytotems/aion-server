#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSnareTurretAI.h"

#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionSnareTurretAI, "eternal_bastion_snare_turret");

// Java EternalBastionSnareTurretAI.java:26-29
model::templates::item::ItemAttackType EternalBastionSnareTurretAI::modifyAttackType(model::templates::item::ItemAttackType type) {
	return model::templates::item::ItemAttackType::MAGICAL_FIRE;
}

// Java EternalBastionSnareTurretAI.java:31-36
void EternalBastionSnareTurretAI::handleFinishAttack() {
	gameserver::ai::manager::EmoteManager::emoteStopAttacking(getOwner());
	getOwner().getController().loseAggro(false);
	getOwner().setSkillNumber(0);
}

// Java EternalBastionSnareTurretAI.java:38-44
void EternalBastionSnareTurretAI::handleCreatureAggro(Creature& creature) {
	EternalBastionAggressiveNpcAI::handleCreatureAggro(creature);
	if (isSkillDisabled.compareAndSet(false, true)) {
		SkillEngine::getInstance().getSkill(getOwner(), 21116, 65, runtime::Ptr<VisibleObject>(creature))->useNoAnimationSkill(); // Magnetic Pull
	}
}

// Java EternalBastionSnareTurretAI.java:46-52
void EternalBastionSnareTurretAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21116) // Magnetic Pull
		SkillEngine::getInstance().getSkill(getOwner(), 21117, 65, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill(); // Blanket Venting
	else if (skillTemplate->getSkillId() == 21117) // Blanket Venting
		SkillEngine::getInstance().applyEffectDirectly(21117, getOwner(), getOwner()); // Work-around since HEROs ignore abnormal states atm
}

// Java EternalBastionSnareTurretAI.java:54-58
void EternalBastionSnareTurretAI::onEffectEnd(runtime::Ptr<Effect> effect) {
	if (effect != nullptr && effect->getSkillId() == 21117)
		isSkillDisabled.set(false);
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
