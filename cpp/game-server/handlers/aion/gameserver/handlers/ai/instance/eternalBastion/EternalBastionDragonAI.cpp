#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionDragonAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/animations/AttackHandAnimation.h"
#include "aion/gameserver/model/animations/AttackTypeAnimation.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionDragonAI, "eternal_bastion_dragon");

// Java EternalBastionDragonAI.java:28-33
model::animations::AttackTypeAnimation EternalBastionDragonAI::getAttackTypeAnimation(Creature& target) {
	dist = PositionUtil::getDistance(getOwner(), target) - getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide()
		- target.getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide();
	return dist.get() > 6 ? model::animations::AttackTypeAnimation::RANGED : model::animations::AttackTypeAnimation::MELEE;
}

// Java EternalBastionDragonAI.java:35-38
float EternalBastionDragonAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return effect == nullptr && dist.get() > 6 ? damage * 1.2f : damage;
}

// Java EternalBastionDragonAI.java:40-43
model::templates::item::ItemAttackType EternalBastionDragonAI::modifyAttackType(model::templates::item::ItemAttackType type) {
	return dist.get() > 6 ? model::templates::item::ItemAttackType::MAGICAL_EARTH : model::templates::item::ItemAttackType::PHYSICAL;
}

// Java EternalBastionDragonAI.java:45-48 (Rnd.get(AttackHandAnimation.values()): one of the constants, by their count)
model::animations::AttackHandAnimation EternalBastionDragonAI::modifyAttackHandAnimation(
	model::animations::AttackHandAnimation attackHandAnimation) {
	return static_cast<model::animations::AttackHandAnimation>(
		commons::utils::Rnd::nextInt(static_cast<int32_t>(xml::EnumTraits<model::animations::AttackHandAnimation>::names.size())));
}

// Java EternalBastionDragonAI.java:50-55
bool EternalBastionDragonAI::ask(AIQuestion question) {
	if (question == AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES)
		return true;
	return EternalBastionAggressiveNpcAI::ask(question);
}

// Java EternalBastionDragonAI.java:57-61
void EternalBastionDragonAI::handleSpawned() {
	EternalBastionAggressiveNpcAI::handleSpawned();
	spawn(284697, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), int8_t{0});
}

// Java EternalBastionDragonAI.java:63-67 (Gnaw's random target may be null: Java's NullPointerException in addHate)
void EternalBastionDragonAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21239) // Gnaw
		getAggroList().addHate(*getAggroList().getTarget(controllers::attack::AggroTarget::RANDOM), 50000);
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
