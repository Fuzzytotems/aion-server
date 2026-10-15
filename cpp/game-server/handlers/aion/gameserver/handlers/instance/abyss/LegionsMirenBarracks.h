#pragma once

#include "aion/gameserver/handlers/instance/abyss/MirenBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * LegionsMirenBarracks (301250000): the legion version of MirenBarracks, with its own chests.
 * <p>
 * Java: data/handlers/instance/abyss/LegionsMirenBarracks.java, @InstanceID(301250000).
 *
 * @author Estrayl
 */
class LegionsMirenBarracks : public MirenBarracks {
	AION_MAKE_REF_FRIEND
protected:
	explicit LegionsMirenBarracks(WorldMapInstance& instance);

	int32_t getChestId() override;

public:
	static runtime::Ref<LegionsMirenBarracks> create(WorldMapInstance& instance);
};

} // namespace aion::gameserver::handlers::instance::abyss
