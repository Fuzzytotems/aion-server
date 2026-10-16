#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSiegeMineAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionSiegeMineAI, "eternal_bastion_siege_mine");

// Java EternalBastionSiegeMineAI.java:24-29
void EternalBastionSiegeMineAI::handleAttack(runtime::Ptr<Creature> creature) {
	EternalBastionConstructAI::handleAttack(creature);
	if (isActivated.compareAndSet(false, true))
		SkillEngine::getInstance().getSkill(getOwner(), 21143, 1, runtime::Ptr<VisibleObject>(getOwner()))->useWithoutPropSkill();
}

// Java EternalBastionSiegeMineAI.java:31-37
void EternalBastionSiegeMineAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21143) {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			SkillEngine::getInstance().getSkill(getOwner(), 20549, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill();
		}, 750);
		spawn(getNpcId() == 831330 ? 284696 : 284695, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ() + 1, int8_t{0});
	}
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
