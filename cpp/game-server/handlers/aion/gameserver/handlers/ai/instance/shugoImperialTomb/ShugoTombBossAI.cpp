#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombBossAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombBossAI, "shugo_tomb_boss");

// Java ShugoTombBossAI.java:35-38
void ShugoTombBossAI::handleSpawned() {
	ShugoTombAttackerAI::handleSpawned();
}

// Java ShugoTombBossAI.java:40-55
void ShugoTombBossAI::handleAttack(runtime::Ptr<Creature> creature) {
	ShugoTombAttackerAI::handleAttack(creature);
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>> skills = getSkillList()->getNpcSkills();
	if (getOwner().getEffectController()->isInAnyAbnormalState(skillengine::effect::AbnormalState::CANT_ATTACK_STATE) ||
		usedSkills.get() >= skills->size())
		return;

	runtime::Ptr<NpcSkillEntry> entry = skills->get(usedSkills.get());
	if (getLifeStats()->getHpPercentage() <= entry->getTemplate()->getMaxhp() && isCasting.compareAndSet(false, true)) {
		if (getMoveController()->isInMove())
			WalkManager::stopWalking(*this);

		SkillEngine::getInstance().getSkill(getOwner(), entry->getSkillId(), entry->getSkillLevel(), runtime::Ptr<VisibleObject>(getOwner()))
			->useWithoutPropSkill();
		usedSkills.incrementAndGet();
	}
}

// Java ShugoTombBossAI.java:57-72 (the lambda pins this)
void ShugoTombBossAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	if (skillTemplate->getSkillId() == 21266) {
		for (int32_t i = 0; i < 4; i++) {
			rndSpawnInRange(219509, 2);
		}
	}
	isCasting.set(false);
	if (!canThink()) {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			WalkManager::startWalking(*this);
			getOwner().setState(CreatureState::WALK_MODE);
			PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
		}, 500);
	}
}

// Java ShugoTombBossAI.java:74-80
bool ShugoTombBossAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_LOOT:
			return true;
		default:
			return ShugoTombAttackerAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
