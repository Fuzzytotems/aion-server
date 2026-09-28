#include "aion/gameserver/controllers/observer/FlyRingObserver.h"

#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::controllers::observer {

using geoEngine::math::Vector3f;
using runtime::Ptr;
using runtime::Ref;

FlyRingObserver::FlyRingObserver(model::flyring::FlyRing& ringValue, model::gameobjects::player::Player& playerValue)
	: ActionObserver(ObserverType::MOVE), player(playerValue), ring(ringValue) {
	oldPosition.set(Vector3f(playerValue.getX(), playerValue.getY(), playerValue.getZ()));
}

FlyRingObserver::~FlyRingObserver() = default;

runtime::Ref<FlyRingObserver> FlyRingObserver::create(model::flyring::FlyRing& ringValue, model::gameobjects::player::Player& playerValue) {
	return runtime::makeRef<FlyRingObserver>(ringValue, playerValue);
}

void FlyRingObserver::moved() {
	Vector3f newPosition(player->getX(), player->getY(), player->getZ());
	if (ring->isCrossed(oldPosition.get(), newPosition)) {
		if (ring->getTemplate()->getMap() == 400010000 || isQuestactive() || isInstancetactive()) {
			const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(265); // Wings of Aether
			Ref<skillengine::model::Effect> speedUp = skillengine::model::Effect::create(*player, player, skillTemplate, skillTemplate->getLvl());
			speedUp->initialize();
			speedUp->addAllEffectToSucess();
			speedUp->applyEffect();
		}
		Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, *player, 0);
		questEngine::QuestEngine::getInstance().onPassFlyingRing(*env, ring->getName());
	}
	oldPosition = newPosition;
}

bool FlyRingObserver::isInstancetactive() {
	return ring->getPosition()->getWorldMapInstance()->getInstanceHandler()->onPassFlyingRing(*player, ring->getName());
}

bool FlyRingObserver::isQuestactive() {
	int32_t questId = player->getRace() == model::Race::ASMODIANS ? 2042 : 1044;
	Ptr<questEngine::model::QuestState> qs = player->getQuestStateList()->getQuestState(questId);

	if (!qs)
		return false;

	return qs->getStatus() == questEngine::model::QuestStatus::START && qs->getQuestVarById(0) >= 2 && qs->getQuestVarById(0) <= 8;
}

} // namespace aion::gameserver::controllers::observer
