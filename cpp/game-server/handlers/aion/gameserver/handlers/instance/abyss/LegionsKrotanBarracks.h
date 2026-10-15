#pragma once

#include "aion/gameserver/handlers/instance/abyss/KrotanBarracks.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * LegionsKrotanBarracks (301260000): the legion version of KrotanBarracks, with its own chests.
 * <p>
 * Java: data/handlers/instance/abyss/LegionsKrotanBarracks.java, @InstanceID(301260000).
 *
 * @author Estrayl
 */
class LegionsKrotanBarracks : public KrotanBarracks {
	AION_MAKE_REF_FRIEND
protected:
	explicit LegionsKrotanBarracks(WorldMapInstance& instance);

	int32_t getChestId() override;

public:
	static runtime::Ref<LegionsKrotanBarracks> create(WorldMapInstance& instance);
};

} // namespace aion::gameserver::handlers::instance::abyss
