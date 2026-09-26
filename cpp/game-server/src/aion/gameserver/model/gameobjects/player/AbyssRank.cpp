#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/gameobjects/player/detail/PlayerMath.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::model::gameobjects::player {

namespace {

using utils::stats::AbyssRankEnum;

/** Java AbyssRankEnum constructor data (id, requiredAP, requiredGP) in ordinal order; the companion belongs to P5-01 and does not exist yet */
struct AbyssRankData {
	int32_t id;
	int32_t requiredAP;
	int32_t requiredGP;
};

constexpr std::array<AbyssRankData, 18> ABYSS_RANK_DATA{{
	{1, 0, 0},      // GRADE9_SOLDIER
	{2, 1200, 0},   // GRADE8_SOLDIER
	{3, 4220, 0},   // GRADE7_SOLDIER
	{4, 10990, 0},  // GRADE6_SOLDIER
	{5, 23500, 0},  // GRADE5_SOLDIER
	{6, 42780, 0},  // GRADE4_SOLDIER
	{7, 69700, 0},  // GRADE3_SOLDIER
	{8, 105600, 0}, // GRADE2_SOLDIER
	{9, 150800, 0}, // GRADE1_SOLDIER
	{10, 0, 1244},  // STAR1_OFFICER
	{11, 0, 1368},  // STAR2_OFFICER
	{12, 0, 1915},  // STAR3_OFFICER
	{13, 0, 3064},  // STAR4_OFFICER
	{14, 0, 5210},  // STAR5_OFFICER
	{15, 0, 8335},  // GENERAL
	{16, 0, 10002}, // GREAT_GENERAL
	{17, 0, 11503}, // COMMANDER
	{18, 0, 12437}, // SUPREME_COMMANDER
}};
static_assert(static_cast<size_t>(AbyssRankEnum::SUPREME_COMMANDER) + 1 == ABYSS_RANK_DATA.size());

const AbyssRankData& rankData(AbyssRankEnum rank) {
	return ABYSS_RANK_DATA[static_cast<size_t>(rank)];
}

/** Java AbyssRankEnum.getRankById(id) */
AbyssRankEnum getRankById(int32_t id) {
	for (size_t i = 0; i < ABYSS_RANK_DATA.size(); ++i) {
		if (ABYSS_RANK_DATA[i].id == id)
			return static_cast<AbyssRankEnum>(i);
	}
	throw runtime::IllegalArgumentException("Invalid abyss rank provided " + std::to_string(id));
}

/** Java AbyssRankEnum.getRankForPoints(ap, gp) */
AbyssRankEnum getRankForPoints(int32_t ap, int32_t gp) {
	AbyssRankEnum r = AbyssRankEnum::GRADE9_SOLDIER;
	for (size_t i = 0; i < ABYSS_RANK_DATA.size(); ++i) {
		if (ABYSS_RANK_DATA[i].requiredAP <= ap && ABYSS_RANK_DATA[i].requiredGP <= gp)
			r = static_cast<AbyssRankEnum>(i);
	}
	return r;
}

/** GSConfig.TIME_ZONE_ID (the system zone before the configuration is loaded) */
const std::chrono::time_zone* serverZone() {
	const std::chrono::time_zone* zone = configs::main::GSConfig::TIME_ZONE_ID.load();
	return zone != nullptr ? zone : std::chrono::current_zone();
}

/** Java int addition (wraps on overflow) */
int32_t addInt(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

AbyssRank::AbyssRank(int32_t dailyAPValue, int32_t weeklyAPValue, int32_t ap, int32_t rankValue, int32_t dailyKillValue, int32_t weeklyKillValue,
	int32_t allKillValue, int32_t maxRankValue, int32_t lastKillValue, int32_t lastAPValue, int64_t lastUpdateValue, int32_t daily_gp,
	int32_t weekly_gp, int32_t gp, int32_t last_gp) {
	// Deviation: Java's persistentState starts as null (only doUpdate or the DAO set it); C++ starts with NOACTION, which setPersistentState
	// treats like null (it is not NEW), docs/deviations/P4-12.md
	persistentState.set(PersistentState::NOACTION);
	dailyAP.set(dailyAPValue);
	weeklyAP.set(weeklyAPValue);
	currentAp.set(ap);
	rank.set(getRankById(rankValue));
	dailyKill.set(dailyKillValue);
	weeklyKill.set(weeklyKillValue);
	allKill.set(allKillValue);
	maxRank.set(maxRankValue);
	lastKill.set(lastKillValue);
	lastAP.set(lastAPValue);
	lastUpdate.set(lastUpdateValue);
	dailyGP.set(daily_gp);
	weeklyGP.set(weekly_gp);
	currentGp.set(gp > 0 ? gp : 0);
	lastGP.set(last_gp);

	doUpdate();
}

AbyssRank::~AbyssRank() = default;

runtime::Ref<AbyssRank> AbyssRank::create(int32_t dailyAPValue, int32_t weeklyAPValue, int32_t ap, int32_t rankValue, int32_t dailyKillValue,
	int32_t weeklyKillValue, int32_t allKillValue, int32_t maxRankValue, int32_t lastKillValue, int32_t lastAPValue, int64_t lastUpdateValue,
	int32_t daily_gp, int32_t weekly_gp, int32_t gp, int32_t last_gp) {
	return runtime::makeRef<AbyssRank>(dailyAPValue, weeklyAPValue, ap, rankValue, dailyKillValue, weeklyKillValue, allKillValue, maxRankValue,
		lastKillValue, lastAPValue, lastUpdateValue, daily_gp, weekly_gp, gp, last_gp);
}

void AbyssRank::addAp(int32_t additionalAp) {
	// java-race: unsynchronized read-modify-write of the AP fields and the rank; AbyssPointsService grants AP from any thread (PvP and NPC kills,
	// quest rewards), so two concurrent grants can lose points or compute the rank from stale values
	if (additionalAp > 0) {
		dailyAP.set(addInt(dailyAP.get(), additionalAp));
		if (dailyAP.get() < 0)
			dailyAP.set(0);

		weeklyAP.set(addInt(weeklyAP.get(), additionalAp));
		if (weeklyAP.get() < 0)
			weeklyAP.set(0);
	}

	int32_t cappedCount;
	if (configs::main::CustomConfig::ENABLE_AP_CAP.load()) {
		const int64_t capValue = configs::main::CustomConfig::AP_CAP_VALUE.load();
		// Java: currentAp + additionalAp > AP_CAP_VALUE ? (int) (AP_CAP_VALUE - currentAp) : additionalAp (int sum, then widened)
		cappedCount = addInt(currentAp.get(), additionalAp) > capValue ? static_cast<int32_t>(capValue - currentAp.get()) : additionalAp;
	} else {
		cappedCount = additionalAp;
	}

	currentAp.set(addInt(currentAp.get(), cappedCount));
	if (currentAp.get() < 0)
		currentAp.set(0);

	AbyssRankEnum newRank = getRankForPoints(currentAp.get(), currentGp.get());
	if (rankData(newRank).requiredGP == 0)
		setRank(newRank);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void AbyssRank::addGp(int32_t amount, bool addToStats) {
	// java-race: unsynchronized read-modify-write of the GP fields (lost GP when two grants race)
	if (addToStats) {
		dailyGP.set(addInt(dailyGP.get(), amount));
		weeklyGP.set(addInt(weeklyGP.get(), amount));
	}
	currentGp.set(addInt(currentGp.get(), amount));
	if (currentGp.get() < 0)
		currentGp.set(0);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void AbyssRank::setRank(utils::stats::AbyssRankEnum rankValue) {
	if (rankData(rankValue).id > maxRank.get())
		maxRank.set(rankData(rankValue).id);

	rank.set(rankValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void AbyssRank::incrementAllKills() {
	dailyKill++;
	weeklyKill++;
	allKill++;
}

void AbyssRank::setPersistentState(PersistentState persistentStateValue) {
	if (persistentStateValue != PersistentState::UPDATE_REQUIRED || persistentState.get() != PersistentState::NEW)
		persistentState.set(persistentStateValue);
}

void AbyssRank::doUpdate() {
	bool needUpdate = false;
	const detail::ServerDate lastTime = detail::serverDateOf(lastUpdate.get(), serverZone());
	const detail::ServerDate now = detail::serverDateOf(commons::utils::currentTimeMillis(), serverZone());

	// Checking the day - month & year are checked to prevent if a player come back after 1 month, the same day
	if (lastTime.date.day() != now.date.day() || lastTime.date.month() != now.date.month() || lastTime.date.year() != now.date.year()) {
		dailyAP.set(0);
		dailyKill.set(0);
		dailyGP.set(0);
		needUpdate = true;
	}

	// Checking the week - year is checked to prevent if a player come back after 1 year, the same week
	if (lastTime.isoWeek != now.isoWeek || lastTime.date.year() != now.date.year()) {
		lastKill.set(weeklyKill.get());
		lastAP.set(weeklyAP.get());
		lastGP.set(weeklyGP.get());
		weeklyKill.set(0);
		weeklyAP.set(0);
		weeklyGP.set(0);
		needUpdate = true;
	}

	// For offline changed ranks
	if (rankData(rank.get()).id > maxRank.get()) {
		maxRank.set(rankData(rank.get()).id);
		needUpdate = true;
	}

	// Finally, update the the last update
	lastUpdate.set(commons::utils::currentTimeMillis());

	if (needUpdate)
		setPersistentState(PersistentState::UPDATE_REQUIRED);
}

} // namespace aion::gameserver::model::gameobjects::player
