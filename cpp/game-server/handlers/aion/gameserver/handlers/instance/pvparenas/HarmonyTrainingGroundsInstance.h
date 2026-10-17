#pragma once

#include "aion/gameserver/handlers/instance/pvparenas/PvPArenaInstance.h"

#include <string_view>
#include <vector>

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * Harmony Training Grounds (300570000): the group arena - a HarmonyArenaScore, the groups' merged damage and rewards, 36 fly rings whose
 * npcs give the group 100 points.
 * <p>
 * Java: data/handlers/instance/pvparenas/HarmonyTrainingGroundsInstance.java, @InstanceID(300570000). Java's casts of instanceScore to
 * HarmonyArenaScore are runtime::cast (createNewArenaScore makes one).
 *
 * @author xTz
 */
class HarmonyTrainingGroundsInstance : public PvPArenaInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit HarmonyTrainingGroundsInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<HarmonyTrainingGroundsInstance> create(WorldMapInstance& instance);

protected:
	int32_t getBoostMoraleEffectDuration(int32_t rank) override;

	void spawnRings() override;

private:
	void spawnFlyRing(std::string_view name, Point3D& center, Point3D& p1, Point3D& p2);

public:
	bool onPassFlyingRing(Player& player, std::string_view flyingRing) override;

protected:
	using PvPArenaInstance::updatePoints;

	void updatePoints(runtime::Ptr<PvPArenaPlayerReward> receiver, Player& player, VisibleObject& victim, int32_t rewardPoints) override;

public:
	void onInstanceCreate() override;

protected:
	using PvPArenaInstance::getRunnerUpScoreMod;

	float getRunnerUpScoreMod(int32_t victimRank) override;

	runtime::Ref<PvPArenaScore> createNewArenaScore() override;

	std::vector<runtime::Ptr<PvPArenaPlayerReward>> getArenaRewards() override;

	runtime::Ptr<PvPArenaPlayerReward> getStatReward(Player& player) override;

	bool canStart() override;

	bool shouldMergeGroupDamage() override;

	void sendEntryPacket(Player& player) override;

	void broadcastUpdate(Player& target, InstanceScoreType scoreType) override;

	void broadcastUpdate(InstanceScoreType scoreType) override;

	void broadcastResults() override;

	void sendPacket(runtime::Ptr<Player> receiver, std::optional<InstanceScoreType> scoreType) override;

private:
	int32_t getTime();

	HarmonyArenaScore& harmonyScore();
};

} // namespace aion::gameserver::handlers::instance::pvparenas
