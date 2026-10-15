#include "aion/gameserver/model/stats/calc/NpcStatCalculation.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npc/NpcRank.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/JavaMath.h"

namespace aion::gameserver::model::stats::calc {

using container::StatEnum;
using templates::npc::NpcRank;
using templates::npc::NpcRating;

int32_t NpcStatCalculation::calculateStat(StatEnum stat, NpcRating rating, NpcRank rank, int8_t level) {
	float baseValue = getBaseValue(stat, level);
	float ratingMod = getRatingModifier(stat, rating);
	float rankMod = getRankModifier(stat, rank);
	return utils::JavaMath::round(baseValue * ratingMod * rankMod);
}

float NpcStatCalculation::getBaseValue(StatEnum stat, int8_t level) {
	// Java: (float) Math.pow(level, n) of a byte is exact in double
	const double lvl = level;
	switch (stat) {
		// https://www.wolframalpha.com/input/?i=-0.0007x%5E3+%2B+0.1x%5E2+%2B+5.3x
		case StatEnum::PHYSICAL_ATTACK: {
			float cube = static_cast<float>(lvl * lvl * lvl);
			float square = static_cast<float>(lvl * lvl);
			float a = -0.0007f * cube;
			float b = 0.1f * square;
			float c = 5.3f * static_cast<float>(level);
			return a + b + c;
		}
		case StatEnum::MAGICAL_DEFEND:
			return static_cast<float>(level) * 5.0f;
		case StatEnum::MAGICAL_ATTACK:
			return static_cast<float>(level) * 20.0f;
		case StatEnum::PHYSICAL_DEFENSE:
			return static_cast<float>(level) * 17.0f;
		case StatEnum::MAGICAL_ACCURACY:
			return static_cast<float>(level) * 25.0f;
		// formula with help of https://www.wolframalpha.com/input/?i=fit+(1,20),(15,270),(30,585),(50,1075),(60,1350),(65,1495)
		case StatEnum::MAGICAL_RESIST: {
			float a = 0.1f * static_cast<float>(lvl * lvl);
			float b = 16.5f * static_cast<float>(level);
			return a + b;
		}
		case StatEnum::PHYSICAL_ACCURACY:
			return static_cast<float>(level) * 37.0f;
		case StatEnum::PARRY:
			return static_cast<float>(level) * 40.0f;
		case StatEnum::PHYSICAL_CRITICAL_RESIST:
			return static_cast<float>(level - 50) * 2.5f;
		case StatEnum::MAGICAL_CRITICAL_RESIST:
			return static_cast<float>(level - 50) * 1.1f;
		case StatEnum::STUNLIKE_RESISTANCE:
			return 100.0f;
		default:
			throw runtime::IllegalArgumentException("Stat calculation for " + std::string(xml::enumName(stat)) + " is not implemented");
	}
}

float NpcStatCalculation::getRatingModifier(StatEnum stat, NpcRating rating) {
	switch (rating) {
		case NpcRating::JUNK:
		case NpcRating::NORMAL:
			switch (stat) {
				case StatEnum::MAGICAL_ATTACK:
					return 0.4f;
				case StatEnum::STUNLIKE_RESISTANCE:
					return 0.0f;
				default:
					return 1.0f;
			}
		case NpcRating::ELITE:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
					return 1.7f;
				case StatEnum::MAGICAL_ATTACK:
					return 0.5f;
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return 1.25f;
				case StatEnum::MAGICAL_RESIST:
					return 1.05f;
				case StatEnum::PHYSICAL_ACCURACY:
				case StatEnum::MAGICAL_ACCURACY:
					return 1.03f;
				case StatEnum::PARRY:
					return 1.025f;
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
					return 9.0f;
				case StatEnum::MAGICAL_CRITICAL_RESIST:
					return 8.5f;
				case StatEnum::STUNLIKE_RESISTANCE:
					return 5.0f;
				default:
					return 1.0f;
			}
		case NpcRating::HERO:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
					return 2.4f;
				case StatEnum::MAGICAL_ATTACK:
					return 0.6f;
				case StatEnum::PHYSICAL_ACCURACY:
				case StatEnum::MAGICAL_ACCURACY:
					return 1.075f;
				case StatEnum::MAGICAL_RESIST:
					return 1.2f;
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return 1.5f;
				case StatEnum::PARRY:
					return 1.07f;
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
				case StatEnum::MAGICAL_CRITICAL_RESIST:
					return 13.5f;
				case StatEnum::STUNLIKE_RESISTANCE:
					return 20.0f;
				default:
					return 1.0f;
			}
		case NpcRating::LEGENDARY:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
					return 2.6f;
				case StatEnum::PHYSICAL_DEFENSE:
				case StatEnum::MAGICAL_DEFEND:
					return 1.75f;
				case StatEnum::MAGICAL_RESIST:
					return 1.35f;
				case StatEnum::MAGICAL_ACCURACY:
					return 1.47f;
				case StatEnum::MAGICAL_ATTACK:
				case StatEnum::PARRY:
				case StatEnum::PHYSICAL_ACCURACY:
					return 1.1f;
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
				case StatEnum::MAGICAL_CRITICAL_RESIST:
					return 13.5f;
				case StatEnum::STUNLIKE_RESISTANCE:
					return 20.0f;
				default:
					return 1.0f;
			}
	}
	throw runtime::IllegalStateException("unknown NpcRating"); // Java: MatchException of an exhaustive switch
}

float NpcStatCalculation::getRankModifier(StatEnum stat, NpcRank rank) {
	switch (rank) {
		case NpcRank::NOVICE:
			switch (stat) {
				case StatEnum::STUNLIKE_RESISTANCE:
					return 0.2f;
				default:
					return 1.0f;
			}
		case NpcRank::DISCIPLINED:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
					return 1.2f;
				case StatEnum::MAGICAL_RESIST:
					return 1.02f;
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return 1.1f;
				case StatEnum::MAGICAL_ATTACK:
					return 1.45f;
				case StatEnum::PARRY:
					return 1.05f;
				case StatEnum::STUNLIKE_RESISTANCE:
					return 0.4f;
				default:
					return 1.0f;
			}
		case NpcRank::SEASONED:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
					return 1.6f;
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return 1.2f;
				case StatEnum::MAGICAL_RESIST:
					return 1.03f;
				case StatEnum::MAGICAL_ATTACK:
					return 1.45f;
				case StatEnum::PARRY:
					return 1.1f;
				case StatEnum::PHYSICAL_ACCURACY:
				case StatEnum::MAGICAL_ACCURACY:
					return 1.01f;
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
					return 1.4f;
				case StatEnum::STUNLIKE_RESISTANCE:
					return 0.6f;
				default:
					return 1.0f;
			}
		case NpcRank::EXPERT:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
					return 1.65f;
				case StatEnum::MAGICAL_RESIST:
					return 1.04f;
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return 1.3f;
				case StatEnum::MAGICAL_ATTACK:
					return 1.7f;
				case StatEnum::PARRY:
					return 1.1f;
				case StatEnum::PHYSICAL_ACCURACY:
				case StatEnum::MAGICAL_ACCURACY:
					return 1.02f;
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
					return 1.6f;
				case StatEnum::MAGICAL_CRITICAL_RESIST:
					return 1.2f;
				default:
					return 1.0f;
			}
		case NpcRank::VETERAN:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::MAGICAL_ATTACK:
					return 1.7f;
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
				case StatEnum::STUNLIKE_RESISTANCE:
					return 1.4f;
				case StatEnum::MAGICAL_RESIST:
					return 1.05f;
				case StatEnum::PARRY:
					return 1.12f;
				case StatEnum::PHYSICAL_ACCURACY:
				case StatEnum::MAGICAL_ACCURACY:
					return 1.03f;
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
					return 1.8f;
				case StatEnum::MAGICAL_CRITICAL_RESIST:
					return 1.25f;
				default:
					return 1.0f;
			}
		case NpcRank::MASTER:
			switch (stat) {
				case StatEnum::PHYSICAL_ATTACK:
					return 1.85f;
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return 1.5f;
				case StatEnum::MAGICAL_RESIST:
					return 1.06f;
				case StatEnum::MAGICAL_ATTACK:
				case StatEnum::STUNLIKE_RESISTANCE:
					return 1.7f;
				case StatEnum::PARRY:
					return 1.12f;
				case StatEnum::PHYSICAL_ACCURACY:
				case StatEnum::MAGICAL_ACCURACY:
					return 1.04f;
				case StatEnum::PHYSICAL_CRITICAL_RESIST:
					return 1.8f;
				case StatEnum::MAGICAL_CRITICAL_RESIST:
					return 1.25f;
				default:
					return 1.0f;
			}
	}
	throw runtime::IllegalStateException("unknown NpcRank"); // Java: MatchException of an exhaustive switch
}

} // namespace aion::gameserver::model::stats::calc
