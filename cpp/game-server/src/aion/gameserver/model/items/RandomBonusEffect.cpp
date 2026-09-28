#include "aion/gameserver/model/items/RandomBonusEffect.h"

#include <memory>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/stats/ModifiersTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::items {

RandomBonusEffect::RandomBonusEffect(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId, int32_t statBonusIdValue)
	: statBonusId(statBonusIdValue) {
	const templates::stats::ModifiersTemplate* bonusTemplate = detail::getRandomBonusTemplate(type, statBonusSetId, statBonusIdValue);
	if (bonusTemplate == nullptr) // Java: NullPointerException on getModifiers() of a missing template
		throw runtime::NullPointerException("ModifiersTemplate");
	for (const std::unique_ptr<stats::calc::functions::StatFunction>& modifier : bonusTemplate->getModifiers())
		this->stats.add(modifier.get());
}

RandomBonusEffect::~RandomBonusEffect() = default;

runtime::Ref<RandomBonusEffect> RandomBonusEffect::create(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId,
	int32_t statBonusIdValue) {
	return runtime::makeRef<RandomBonusEffect>(type, statBonusSetId, statBonusIdValue);
}

void RandomBonusEffect::applyEffect(gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<stats::calc::functions::IStatFunction>> functions;
	for (const stats::calc::functions::StatFunction* modifier : stats.snapshot())
		functions.push_back(stats::calc::functions::StatFunction::ofTemplate(modifier));
	player.getGameStats()->addEffect(runtime::Ptr<stats::calc::StatOwner>(static_cast<stats::calc::StatOwner&>(*this)), functions);
}

void RandomBonusEffect::endEffect(gameobjects::player::Player& player) {
	player.getGameStats()->endEffect(*this);
}

} // namespace aion::gameserver::model::items
