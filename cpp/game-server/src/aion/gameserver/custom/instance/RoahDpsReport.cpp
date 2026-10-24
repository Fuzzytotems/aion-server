#include "aion/gameserver/custom/instance/RoahDpsReport.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace aion::gameserver::custom::instance {

RoahDpsReport::RoahDpsReport(Clock clock) : clock(std::move(clock)) {
}

void RoahDpsReport::runStarted() {
	runStart.set(clock());
}

void RoahDpsReport::artifactDestroyed(float seconds, int32_t dps, int32_t classAverageDps, bool wasPrebuffed) {
	artifactSeconds.set(seconds);
	artifactDps.set(dps);
	avgClassDps.set(classAverageDps);
	prebuffed.set(wasPrebuffed);
}

void RoahDpsReport::bossSpawned() {
	bossStart.set(clock());
}

void RoahDpsReport::bossKilled(int64_t bossMaxHp) {
	const int64_t now = clock();
	runEnd.set(now);
	const int64_t millis = now - bossStart.get().value_or(now);
	bossSeconds.set(static_cast<float>(millis) / 1000.0f);
	bossDps.set(dps(bossMaxHp, millis));
}

void RoahDpsReport::runEnded() {
	if (!runEnd.get())
		runEnd.set(clock());
}

void RoahDpsReport::addDamage(int64_t damage) {
	if (bossStart.get())
		bossPhaseDamage.addAndGet(damage);
	else
		artifactPhaseDamage.addAndGet(damage);
}

int32_t RoahDpsReport::getArtifactPhaseTotalDps() const {
	const std::optional<float> artifactTime = artifactSeconds.get();
	if (!artifactTime)
		return 0;
	return dps(artifactPhaseDamage.get(), std::llround(static_cast<double>(*artifactTime) * 1000.0));
}

int32_t RoahDpsReport::getBossPhaseTotalDps() const {
	const std::optional<float> bossTime = bossSeconds.get();
	if (!bossTime)
		return 0;
	return dps(bossPhaseDamage.get(), std::llround(static_cast<double>(*bossTime) * 1000.0));
}

int32_t RoahDpsReport::getRunTotalDps() const {
	return dps(artifactPhaseDamage.get() + bossPhaseDamage.get(), std::llround(static_cast<double>(getTotalSeconds()) * 1000.0));
}

float RoahDpsReport::getTotalSeconds() const {
	const std::optional<int64_t> start = runStart.get();
	if (!start)
		return 0.0f;
	return static_cast<float>(runEnd.get().value_or(clock()) - *start) / 1000.0f;
}

int32_t RoahDpsReport::dps(int64_t damage, int64_t millis) {
	const float secondsUsed = static_cast<float>(std::max<int64_t>(millis, 1)) / 1000.0f;
	return static_cast<int32_t>(std::floor(static_cast<float>(damage) / secondsUsed + 0.5f));
}

std::string RoahDpsReport::seconds(float value) {
	return std::format("{:.1f}s", value);
}

std::string RoahDpsReport::artifactMessage() const {
	return std::format("Aether Stone destroyed in {}: {} DPS on the stone, {} total DPS (class average {}, prebuffed: {}).",
		seconds(artifactSeconds.get().value_or(0.0f)), artifactDps.get(), getArtifactPhaseTotalDps(), avgClassDps.get(),
		prebuffed.get() ? "yes" : "no");
}

std::string RoahDpsReport::artifactPart() const {
	const std::optional<float> artifactTime = artifactSeconds.get();
	if (!artifactTime)
		return "Aether Stone: not destroyed";
	return std::format("Aether Stone: {}, {} DPS on the stone, {} total DPS (class average {})", seconds(*artifactTime), artifactDps.get(),
		getArtifactPhaseTotalDps(), avgClassDps.get());
}

std::string RoahDpsReport::successReport(std::string_view oldRank, std::string_view newRank) const {
	return std::format("[Eternal Challenge] Success | {} | Boss: {}, {} DPS on the boss, {} total DPS | Total: {}, {} total DPS | Rank: {} -> {}",
		artifactPart(), seconds(getBossSeconds()), getBossDps(), getBossPhaseTotalDps(), seconds(getTotalSeconds()), getRunTotalDps(), oldRank,
		newRank);
}

std::string RoahDpsReport::failureReport(std::string_view reason) const {
	return std::format("[Eternal Challenge] Failed ({}) | {} | Time spent: {}, {} total DPS", reason, artifactPart(), seconds(getTotalSeconds()),
		getRunTotalDps());
}

} // namespace aion::gameserver::custom::instance
