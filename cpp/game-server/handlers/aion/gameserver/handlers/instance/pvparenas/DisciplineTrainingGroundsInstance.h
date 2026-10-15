#pragma once

#include "aion/gameserver/handlers/instance/pvparenas/PvPArenaInstance.h"

namespace aion::gameserver::handlers::instance::pvparenas {

/**
 * Discipline Training Grounds (300430000): the one-on-one training arena, 200 points a kill, -100 a death, a score gap of 1500.
 * <p>
 * Java: data/handlers/instance/pvparenas/DisciplineTrainingGroundsInstance.java, @InstanceID(300430000).
 *
 * @author xTz
 */
class DisciplineTrainingGroundsInstance : public PvPArenaInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit DisciplineTrainingGroundsInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DisciplineTrainingGroundsInstance> create(WorldMapInstance& instance);

protected:
	void setScoreCaps() override;

public:
	void onInstanceCreate() override;

protected:
	int32_t getBoostMoraleEffectDuration(int32_t rank) override;

	void sendPacket(runtime::Ptr<Player> player, std::optional<InstanceScoreType> scoreType) override;
};

} // namespace aion::gameserver::handlers::instance::pvparenas
