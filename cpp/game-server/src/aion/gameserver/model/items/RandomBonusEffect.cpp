#include "aion/gameserver/model/items/RandomBonusEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::items {

RandomBonusEffect::RandomBonusEffect(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId, int32_t statBonusIdValue)
	: statBonusId(statBonusIdValue) {
	// Java: this.stats = DataManager.ITEM_RANDOM_BONUSES.getTemplate(type, statBonusSetId, statBonusId).getModifiers()
	static_cast<void>(type);
	static_cast<void>(statBonusSetId);
	AION_UNPORTED();
}

RandomBonusEffect::~RandomBonusEffect() = default;

runtime::Ref<RandomBonusEffect> RandomBonusEffect::create(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId,
	int32_t statBonusIdValue) {
	return runtime::makeRef<RandomBonusEffect>(type, statBonusSetId, statBonusIdValue);
}

void RandomBonusEffect::applyEffect(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void RandomBonusEffect::endEffect(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items
