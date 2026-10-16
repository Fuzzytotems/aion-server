#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionCommanderPashidAI.h"

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
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionCommanderPashidAI, "eternal_bastion_commander_pashid");

// Java EternalBastionCommanderPashidAI.java:30-38
void EternalBastionCommanderPashidAI::handleSpawned() {
	EternalBastionAggressiveNpcAI::handleSpawned();
	spawn(284697, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), int8_t{0});
	commander.set(getPosition()->getWorldMapInstance()->getNpc(209516));
	if (commander.get() == nullptr)
		commander.set(getPosition()->getWorldMapInstance()->getNpc(209517));
	hateCommander(500000);
}

// Java EternalBastionCommanderPashidAI.java:40-43
void EternalBastionCommanderPashidAI::hateCommander(int32_t hate) {
	runtime::Ptr<Npc> c = commander.get();
	if (c != nullptr && !getOwner().getAggroList().isHating(*c) && isInRange(*c, 20))
		getOwner().getAggroList().addHate(*c, hate);
}

// Java EternalBastionCommanderPashidAI.java:49-53
void EternalBastionCommanderPashidAI::handleTargetChanged(Creature& creature) {
	EternalBastionAggressiveNpcAI::handleTargetChanged(creature);
	ThreadPoolManager::getInstance().schedule({this}, [this] { hateCommander(2000000); }, 2000);
}

// Java EternalBastionCommanderPashidAI.java:55-60
model::animations::AttackTypeAnimation EternalBastionCommanderPashidAI::getAttackTypeAnimation(Creature& target) {
	dist = PositionUtil::getDistance(getOwner(), target) - getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide()
		- target.getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide();
	return dist.get() > 10 ? model::animations::AttackTypeAnimation::RANGED : model::animations::AttackTypeAnimation::MELEE;
}

// Java EternalBastionCommanderPashidAI.java:62-65
float EternalBastionCommanderPashidAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return effect == nullptr && dist.get() > 10 ? damage * 1.3f : damage;
}

// Java EternalBastionCommanderPashidAI.java:67-70
model::templates::item::ItemAttackType EternalBastionCommanderPashidAI::modifyAttackType(model::templates::item::ItemAttackType type) {
	return dist.get() > 10 ? model::templates::item::ItemAttackType::MAGICAL_EARTH : model::templates::item::ItemAttackType::PHYSICAL;
}

// Java EternalBastionCommanderPashidAI.java:72-75 (Rnd.get(AttackHandAnimation.values()): one of the constants, by their count)
model::animations::AttackHandAnimation EternalBastionCommanderPashidAI::modifyAttackHandAnimation(
	model::animations::AttackHandAnimation attackHandAnimation) {
	return static_cast<model::animations::AttackHandAnimation>(
		commons::utils::Rnd::nextInt(static_cast<int32_t>(xml::EnumTraits<model::animations::AttackHandAnimation>::names.size())));
}

// Java EternalBastionCommanderPashidAI.java:77-83 (Gnaw's random target may be null: Java's NullPointerException in addHate)
void EternalBastionCommanderPashidAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 21239: // Gnaw
			getAggroList().addHate(*getAggroList().getTarget(controllers::attack::AggroTarget::RANDOM), 100000);
			break;
		case 21236: // Exultation
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500757);
			break;
	}
}

// Java EternalBastionCommanderPashidAI.java:85-89
void EternalBastionCommanderPashidAI::handleDied() {
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500759);
	EternalBastionAggressiveNpcAI::handleDied();
}

// Java EternalBastionCommanderPashidAI.java:91-95
void EternalBastionCommanderPashidAI::handleDespawned() {
	EternalBastionAggressiveNpcAI::handleDespawned();
	commander.set(nullptr);
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
