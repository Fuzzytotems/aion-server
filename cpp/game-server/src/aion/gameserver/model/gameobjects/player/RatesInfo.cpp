#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::player {

namespace {

using configs::main::RatesConfig;
using stats::container::StatEnum;

/** Java (long) floatValue: NaN is 0, out-of-range values saturate */
int64_t toLong(float value) {
	if (std::isnan(value))
		return 0;
	if (value >= 9223372036854775807.0f)
		return std::numeric_limits<int64_t>::max();
	if (value <= -9223372036854775808.0f)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(value);
}

/** Java Rates.get(player, RatesConfig.X) on a snapshot of the reloadable config value */
float rate(Player& player, const commons::configuration::ConfigValue<std::vector<float>>& membershipRates) {
	std::shared_ptr<const std::vector<float>> rates = membershipRates.get();
	return get(player, rates ? *rates : std::vector<float>());
}

/** Java Rates.calcXpRate(player, membershipRates, boostRate) */
float calcXpRate(Player& player, const commons::configuration::ConfigValue<std::vector<float>>& membershipRates, StatEnum boostRate) {
	float endRate = rate(player, membershipRates);
	endRate *= static_cast<float>(player.getGameStats()->getStat(boostRate, 100)->getCurrent()) / 100.0f;
	if (player.isLegionMember() && player.getLegion()->hasBonus())
		endRate *= 1.1f;
	return endRate;
}

/** Java: player.getGameStats().getStat(StatEnum.AP_BOOST, 100).getCurrent() / 100f */
float apBoostRate(Player& player) {
	return static_cast<float>(player.getGameStats()->getStat(StatEnum::AP_BOOST, 100)->getCurrent()) / 100.0f;
}

} // namespace

int64_t calcResult(Rates rates, Player& player, int64_t value) {
	// Java long * float is a float multiplication, the (long) cast saturates
	const auto fvalue = static_cast<float>(value);
	switch (rates) {
		case Rates::XP_HUNTING:
			return toLong(std::min(fvalue * calcXpRate(player, RatesConfig::XP_SOLO_RATES, StatEnum::BOOST_HUNTING_XP_RATE),
				static_cast<float>(player.getCommonData()->getExpNeed()) * 0.2f));
		case Rates::XP_GROUP_HUNTING:
			return toLong(std::min(fvalue * calcXpRate(player, RatesConfig::XP_GROUP_RATES, StatEnum::BOOST_GROUP_HUNTING_XP_RATE),
				static_cast<float>(player.getCommonData()->getExpNeed()) * 0.2f));
		case Rates::XP_QUEST:
			return toLong(fvalue * calcXpRate(player, RatesConfig::XP_QUEST_RATES, StatEnum::BOOST_QUEST_XP_RATE));
		case Rates::XP_GATHERING:
			return toLong(fvalue * calcXpRate(player, RatesConfig::XP_GATHERING_RATES, StatEnum::BOOST_GATHERING_XP_RATE));
		case Rates::XP_CRAFTING:
			return toLong(fvalue * calcXpRate(player, RatesConfig::XP_CRAFTING_RATES, StatEnum::BOOST_CRAFTING_XP_RATE));
		case Rates::XP_PVP:
			return toLong(fvalue * rate(player, RatesConfig::XP_PVP_RATES));
		case Rates::SKILL_XP_GATHERING:
			return toLong(fvalue * rate(player, RatesConfig::SKILL_XP_GATHERING_RATES));
		case Rates::SKILL_XP_CRAFTING:
			return toLong(fvalue * rate(player, RatesConfig::SKILL_XP_CRAFTING_RATES));
		case Rates::AP_PVP: {
			const float statRate = apBoostRate(player);
			return toLong(fvalue * rate(player, RatesConfig::AP_PVP_RATES) * statRate);
		}
		case Rates::AP_PVP_LOST:
			return toLong(fvalue * rate(player, RatesConfig::AP_PVP_LOSS_RATES));
		case Rates::AP_PVE: {
			const float statRate = apBoostRate(player);
			return toLong(fvalue * rate(player, RatesConfig::AP_PVE_RATES) * statRate);
		}
		case Rates::AP_QUEST:
			return toLong(fvalue * rate(player, RatesConfig::AP_QUEST_RATES));
		case Rates::AP_DREDGION:
			return toLong(fvalue * rate(player, RatesConfig::AP_DREDGION_RATES));
		case Rates::GP:
			return toLong(fvalue * rate(player, RatesConfig::GP_RATES));
		case Rates::DP_PVE:
			return toLong(fvalue * rate(player, RatesConfig::DP_PVE_RATES));
		case Rates::DP_PVP:
			return toLong(fvalue * rate(player, RatesConfig::DP_PVP_RATES));
		case Rates::QUEST_KINAH:
			return toLong(fvalue * rate(player, RatesConfig::QUEST_KINAH_RATES));
		case Rates::GATHERING_COUNT:
			return toLong(fvalue * rate(player, RatesConfig::GATHERING_COUNT_RATES));
		case Rates::SELL_LIMIT:
			return toLong(fvalue * rate(player, RatesConfig::SELL_LIMIT_RATES));
	}
	throw runtime::IllegalArgumentException("Unknown Rates constant " + std::to_string(static_cast<int32_t>(rates)));
}

int32_t calcResult(Rates rates, Player& player, int32_t value) {
	int64_t result = calcResult(rates, player, static_cast<int64_t>(value));
	if (result >= std::numeric_limits<int32_t>::min() && result <= std::numeric_limits<int32_t>::max())
		return static_cast<int32_t>(result);
	// Java: Math.toIntExact throws ArithmeticException("integer overflow"); the logger of the constant's class is Rates$<ordinal + 1>
	const std::string constantClass = "com.aionemu.gameserver.model.gameobjects.player.Rates$" + std::to_string(static_cast<int32_t>(rates) + 1);
	commons::logging::LoggerFactory::getLogger(constantClass)
		.error(std::string(xml::EnumTraits<Rates>::names[static_cast<size_t>(rates)]) + " result is too large for " + player.toString() + ": "
				+ std::to_string(result),
			runtime::ArithmeticException("integer overflow"));
	return value;
}

float get(Player& player, const std::vector<float>& membershipRates) {
	if (membershipRates.empty()) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.Rates")
			.warn("Missing rates", runtime::IllegalStateException(""));
		return 1;
	}
	const int32_t membershipLevel = player.getAccount()->getMembership();
	return membershipRates[static_cast<size_t>(std::min(static_cast<int32_t>(membershipRates.size()) - 1, membershipLevel))];
}

} // namespace aion::gameserver::model::gameobjects::player
