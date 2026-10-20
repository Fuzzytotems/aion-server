#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombTransformationDeviceAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombTransformationDeviceAI, "shugo_tomb_transformation_device");

// Java ShugoTombTransformationDeviceAI.java:22-26
void ShugoTombTransformationDeviceAI::handleUseItemFinish(Player& player) {
	applyTransformationEffect(player);
	ActionItemNpcAI::handleUseItemFinish(player);
}

// Java ShugoTombTransformationDeviceAI.java:28-36 (the lambda pins the player)
void ShugoTombTransformationDeviceAI::applyTransformationEffect(Player& player) {
	int32_t skillId;
	switch (getNpcId()) {
		case 831097:
			skillId = player.getRace() == Race::ASMODIANS ? 21105 : 21096;
			break;
		case 831096:
			skillId = player.getRace() == Race::ASMODIANS ? 21104 : 21095;
			break;
		default:
			skillId = 0;
			break;
	}
	if (skillId != 0)
		ThreadPoolManager::getInstance().schedule({&player}, [&player, skillId] { SkillEngine::getInstance().applyEffectDirectly(skillId, player, player); },
			1000);
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
