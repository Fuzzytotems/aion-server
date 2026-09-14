#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

AbyssRank::AbyssRank(int32_t dailyAPValue, int32_t weeklyAPValue, int32_t ap, int32_t rankValue, int32_t dailyKillValue, int32_t weeklyKillValue,
	int32_t allKillValue, int32_t maxRankValue, int32_t lastKillValue, int32_t lastAPValue, int64_t lastUpdateValue, int32_t daily_gp,
	int32_t weekly_gp, int32_t gp, int32_t last_gp) {
	dailyAP.set(dailyAPValue);
	weeklyAP.set(weeklyAPValue);
	currentAp.set(ap);
	// Java: this.rank = AbyssRankEnum.getRankById(rank) (enum companion, not ported)
	static_cast<void>(rankValue);
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
	// Java: doUpdate()
	AION_UNPORTED();
}

AbyssRank::~AbyssRank() = default;

runtime::Ref<AbyssRank> AbyssRank::create(int32_t dailyAPValue, int32_t weeklyAPValue, int32_t ap, int32_t rankValue, int32_t dailyKillValue,
	int32_t weeklyKillValue, int32_t allKillValue, int32_t maxRankValue, int32_t lastKillValue, int32_t lastAPValue, int64_t lastUpdateValue,
	int32_t daily_gp, int32_t weekly_gp, int32_t gp, int32_t last_gp) {
	return runtime::makeRef<AbyssRank>(dailyAPValue, weeklyAPValue, ap, rankValue, dailyKillValue, weeklyKillValue, allKillValue, maxRankValue,
		lastKillValue, lastAPValue, lastUpdateValue, daily_gp, weekly_gp, gp, last_gp);
}

void AbyssRank::addAp(int32_t additionalAp) {
	AION_UNPORTED();
}

void AbyssRank::addGp(int32_t amount, bool addToStats) {
	AION_UNPORTED();
}

void AbyssRank::setRank(utils::stats::AbyssRankEnum rankValue) {
	AION_UNPORTED();
}

void AbyssRank::incrementAllKills() {
	AION_UNPORTED();
}

void AbyssRank::setPersistentState(PersistentState persistentStateValue) {
	AION_UNPORTED();
}

void AbyssRank::doUpdate() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
