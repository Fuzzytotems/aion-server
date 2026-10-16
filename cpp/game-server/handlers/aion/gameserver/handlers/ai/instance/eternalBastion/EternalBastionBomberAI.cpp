#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionBomberAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionBomberAI, "eternal_bastion_bomber");

// Java EternalBastionBomberAI.java:29-33
void EternalBastionBomberAI::handleSpawned() {
	EternalBastionAggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { startWalking(); }, 3000);
}

// Java EternalBastionBomberAI.java:35-39
void EternalBastionBomberAI::startWalking() {
	gameserver::ai::manager::WalkManager::startWalking(*this);
	getOwner().unsetState(CreatureState::WALK_MODE);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
}

// Java EternalBastionBomberAI.java:41-46
void EternalBastionBomberAI::handleAttack(runtime::Ptr<Creature> creature) {
	EternalBastionAggressiveNpcAI::handleAttack(creature);
	if (isExplosionActivated.compareAndSet(false, true))
		SkillEngine::getInstance().getSkill(getOwner(), 21143, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill();
}

// Java EternalBastionBomberAI.java:48-52
void EternalBastionBomberAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21143)
		isExplosionActivated.set(true);
}

// Java EternalBastionBomberAI.java:54-61
void EternalBastionBomberAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21143) {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			SkillEngine::getInstance().getSkill(getOwner(), 20549, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill();
		}, 750); // Self-Destruction
		spawn(284707, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), int8_t{0}); // deals ~500k damage to gates
		spawn(284707, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), int8_t{0}); // not 100% sure about this second one
	}
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
