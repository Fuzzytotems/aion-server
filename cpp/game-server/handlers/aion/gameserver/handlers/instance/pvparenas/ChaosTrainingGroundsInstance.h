#pragma once

#include "aion/gameserver/handlers/instance/pvparenas/PvPArenaInstance.h"

#include <string_view>

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * Chaos Training Grounds (300420000): the free-for-all training arena, 1000 points a kill, -125 a death; the gathered objects, the flame
 * loops and the aether vortices of its rings.
 * <p>
 * Java: data/handlers/instance/pvparenas/ChaosTrainingGroundsInstance.java, @InstanceID(300420000).
 *
 * @author xTz
 */
class ChaosTrainingGroundsInstance : public PvPArenaInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit ChaosTrainingGroundsInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<ChaosTrainingGroundsInstance> create(WorldMapInstance& instance);

public:
	void onInstanceCreate() override;

protected:
	int32_t getBoostMoraleEffectDuration(int32_t rank) override;

	float getRunnerUpScoreMod(int32_t victimRank) override;

	using PvPArenaInstance::getRunnerUpScoreMod;

	void sendPacket(runtime::Ptr<Player> player, std::optional<InstanceScoreType> scoreType) override;

public:
	void onGather(Player& player, Gatherable& gatherable) override;

protected:
	void spawnRings() override;

private:
	void spawnFlyRing(std::string_view name, Point3D& center, Point3D& p1, Point3D& p2);

public:
	bool onPassFlyingRing(Player& player, std::string_view flyingRing) override;
};

} // namespace aion::gameserver::handlers::instance::pvparenas
