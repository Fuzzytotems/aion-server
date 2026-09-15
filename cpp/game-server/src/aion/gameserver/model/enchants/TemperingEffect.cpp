#include "aion/gameserver/model/enchants/TemperingEffect.h"

#include <string>
#include <unordered_map>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/enchants/TemperingStat.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlumStatEnum.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::enchants {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.enchants.TemperingEffect");

namespace {

using stats::calc::functions::IStatFunction;
using stats::calc::functions::RcStatFunction;
using stats::calc::functions::StatAddFunction;
using stats::container::PlumStatEnum;
using stats::container::StatEnum;

/** Java PlumStatEnum.getBoostValue(); the PlumStatEnum companion belongs to the stats chunk (P5-01) */
constexpr int32_t boostValueOf(PlumStatEnum plumStat) noexcept {
	switch (plumStat) {
		case PlumStatEnum::PLUM_HP:
			return 150;
		case PlumStatEnum::PLUM_BOOST_MAGICAL_SKILL:
			return 20;
		case PlumStatEnum::PLUM_PHISICAL_ATTACK:
			return 4;
		case PlumStatEnum::PLUM_SPEED:
			return 0;
	}
	return 0;
}

/** Java int arithmetic (two's complement wrap-around) */
constexpr int32_t javaMul(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

constexpr int32_t javaAdd(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

TemperingEffect::TemperingEffect(gameobjects::player::Player& player, const std::vector<runtime::Ref<IStatFunction>>& functions) {
	std::vector<runtime::Ptr<IStatFunction>> borrowed(functions.begin(), functions.end());
	player.getGameStats()->addEffect(runtime::Ptr<stats::calc::StatOwner>(static_cast<stats::calc::StatOwner&>(*this)), borrowed);
}

TemperingEffect::~TemperingEffect() = default;

runtime::Ref<TemperingEffect> TemperingEffect::create(gameobjects::player::Player& player,
	const std::vector<runtime::Ref<IStatFunction>>& functions) {
	return runtime::makeRef<TemperingEffect>(player, functions);
}

void TemperingEffect::endEffect(gameobjects::player::Player& player) {
	player.getGameStats()->endEffect(*this);
}

void TemperingEffect::addAccessoryStatFunctions(gameobjects::Item& item, std::vector<runtime::Ref<IStatFunction>>& functions) {
	const items::detail::TemperingTemplates* tempering = items::detail::getTemperingTemplates(item.getItemTemplate());
	const std::vector<TemperingStat>* temperingStats = nullptr;
	if (tempering != nullptr) {
		auto level = tempering->find(item.getTempering());
		if (level != tempering->end())
			temperingStats = level->second;
	}
	if (temperingStats == nullptr)
		return;
	for (const TemperingStat& temperingStat : *temperingStats)
		functions.push_back(RcStatFunction<StatAddFunction>::create(temperingStat.getStat(), temperingStat.getValue(), false));
}

void TemperingEffect::addPlumeStatFunctions(gameobjects::Item& item, std::vector<runtime::Ref<IStatFunction>>& functions) {
	StatEnum st;
	int32_t value = item.getRndPlumeBonusValue();
	const std::string& temperingName = item.getItemTemplate()->getTemperingName();
	if (temperingName.empty()) // Java: getTemperingName() is null without the attribute (13 plumes of the static data) and equals() throws
		throw runtime::NullPointerException("temperingName");
	if (temperingName == "TSHIRT_PHYSICAL") {
		st = StatEnum::PHYSICAL_ATTACK;
		value = javaAdd(value, javaMul(boostValueOf(PlumStatEnum::PLUM_PHISICAL_ATTACK), item.getTempering()));
	} else {
		st = StatEnum::BOOST_MAGICAL_SKILL;
		value = javaAdd(value, javaMul(boostValueOf(PlumStatEnum::PLUM_BOOST_MAGICAL_SKILL), item.getTempering()));
	}
	functions.push_back(RcStatFunction<StatAddFunction>::create(st, value, true));
	int32_t hp = javaMul(boostValueOf(PlumStatEnum::PLUM_HP), item.getTempering());
	functions.push_back(RcStatFunction<StatAddFunction>::create(StatEnum::MAXHP, hp, true));
}

void TemperingEffect::apply(gameobjects::player::Player& player, gameobjects::Item& item) {
	std::vector<runtime::Ref<IStatFunction>> functions;
	if (item.getItemTemplate()->getItemGroup() == templates::item::enums::ItemGroup::PLUME) {
		addPlumeStatFunctions(item, functions);
	} else {
		addAccessoryStatFunctions(item, functions);
	}
	if (functions.empty()) {
		log.warn("Missing tempering effect info for item " + item.toString());
		return;
	}
	if (runtime::Ptr<TemperingEffect> temperingEffect = item.getTemperingEffect())
		temperingEffect->endEffect(player);
	item.setTemperingEffect(create(player, functions));
}

} // namespace aion::gameserver::model::enchants
