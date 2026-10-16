#include "aion/gameserver/skillengine/effect/PolymorphEffect.h"

#include <optional>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using runtime::Ptr;

void PolymorphEffect::startEffect(model::Effect& effect) const {
	TransformEffect::startEffect(effect);
	if (model > 0) {
		Ptr<Creature> effected = effect.getEffected();
		const gameserver::model::templates::npc::NpcTemplate* npcTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(model);
		if (npcTemplate != nullptr)
			effected->getTransformModel().setTribe(npcTemplate->getTribe());
	}
}

void PolymorphEffect::endEffect(model::Effect& effect) const {
	TransformEffect::endEffect(effect);
	effect.getEffected()->getTransformModel().setTribe(std::nullopt);
}

} // namespace aion::gameserver::skillengine::effect
