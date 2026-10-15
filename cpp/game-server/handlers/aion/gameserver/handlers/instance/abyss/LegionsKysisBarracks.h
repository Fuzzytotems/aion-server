#pragma once

#include "aion/gameserver/handlers/instance/abyss/KysisBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * LegionsKysisBarracks (301240000): the legion version of KysisBarracks, with its own chests.
 * <p>
 * Java: data/handlers/instance/abyss/LegionsKysisBarracks.java, @InstanceID(301240000).
 *
 * @author Estrayl
 */
class LegionsKysisBarracks : public KysisBarracks {
	AION_MAKE_REF_FRIEND
protected:
	explicit LegionsKysisBarracks(WorldMapInstance& instance);

	int32_t getChestId() override;

public:
	static runtime::Ref<LegionsKysisBarracks> create(WorldMapInstance& instance);
};

} // namespace aion::gameserver::handlers::instance::abyss
