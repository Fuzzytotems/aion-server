#include "aion/gameserver/model/enchants/TemperingEffect.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"

namespace aion::gameserver::model::enchants {

[[maybe_unused]] static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.enchants.TemperingEffect");

TemperingEffect::TemperingEffect(gameobjects::player::Player& player,
	const std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions) {
	// Java: player.getGameStats().addEffect(this, functions)
	AION_UNPORTED();
}

TemperingEffect::~TemperingEffect() = default;

runtime::Ref<TemperingEffect> TemperingEffect::create(gameobjects::player::Player& player,
	const std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions) {
	return runtime::makeRef<TemperingEffect>(player, functions);
}

void TemperingEffect::endEffect(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TemperingEffect::addAccessoryStatFunctions(gameobjects::Item& item,
	std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions) {
	AION_UNPORTED();
}

void TemperingEffect::addPlumeStatFunctions(gameobjects::Item& item, std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions) {
	AION_UNPORTED();
}

void TemperingEffect::apply(gameobjects::player::Player& player, gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::enchants
