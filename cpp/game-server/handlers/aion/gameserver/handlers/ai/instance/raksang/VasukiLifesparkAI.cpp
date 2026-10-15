#include "aion/gameserver/handlers/ai/instance/raksang/VasukiLifesparkAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(VasukiLifesparkAI, "vasuki_lifespark");

// Java VasukiLifesparkAI.java:31-34
bool VasukiLifesparkAI::canThink() {
	return think_.get();
}

// Java VasukiLifesparkAI.java:36-47
void VasukiLifesparkAI::handleSpawned() {
	if (getNpcId() == 217764) {
		think_ = true;
	} else {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			if (!isDead())
				SkillEngine::getInstance().getSkill(getOwner(), 19126, 46, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		}, 3000);
	}
	AggressiveNpcAI::handleSpawned();
}

// Java VasukiLifesparkAI.java:49-103
void VasukiLifesparkAI::handleCreatureMoved(Creature& creature) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature))) {
		if (PositionUtil::getDistance(getOwner(), *player) <= 30) {
			if (startedEvent.compareAndSet(false, true)) {
				int32_t level;
				int32_t shoutId;
				int32_t skill;
				switch (getNpcId()) {
					case 217760:
						skill = 19972;
						level = 45;
						shoutId = 1401107;
						break;
					case 217761:
						skill = 19972;
						level = 46;
						shoutId = 1401171;
						break;
					case 217763:
						skill = 20087;
						level = 46;
						shoutId = 0;
						break;
					default:
						skill = 20039;
						level = 46;
						shoutId = 1401110;
						break;
				}
				if (shoutId != 0) {
					PacketSendUtility::broadcastMessage(getOwner(), shoutId);
				}
				SkillEngine::getInstance().getSkill(getOwner(), skill, level, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
				if (getNpcId() != 217764) {
					ThreadPoolManager::getInstance().schedule({this}, [this, level] {
						if (!isDead()) {
							if (getNpcId() == 217763) {
								getPosition()->getWorldMapInstance()->setDoorState(219, true);
							}
							SkillEngine::getInstance().getSkill(getOwner(), 19967, level, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
							ThreadPoolManager::getInstance().schedule({this}, [this] {
								if (!isDead())
									AIActions::deleteOwner(*this);
							}, 3500);

						}
					}, 3500);
				} else {
					SkillEngine::getInstance().getSkill(getOwner(), 19974, 46, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
				}
			}
		}
	}
}

// Java VasukiLifesparkAI.java:105-111
bool VasukiLifesparkAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java VasukiLifesparkAI.java:113-128
void VasukiLifesparkAI::handleDied() {
	if (getNpcId() == 217764) {
		PacketSendUtility::broadcastMessage(getOwner(), 1401111);
		runtime::Ptr<Npc> soul = getPosition()->getWorldMapInstance()->getNpc(217471);
		runtime::Ptr<Npc> sapping = getPosition()->getWorldMapInstance()->getNpc(217472);
		if (soul != nullptr) {
			soul->getEffectController()->removeEffect(19126);
		}
		if (sapping != nullptr) {
			sapping->getEffectController()->removeEffect(19126);
		}
		PacketSendUtility::broadcastToMap(getOwner(), 1401140);
	}
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
