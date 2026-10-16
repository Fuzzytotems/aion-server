#pragma once

#include "aion/gameserver/handlers/instance/DanuarReliquaryInstance.h"

namespace aion::gameserver::handlers::instance {

/**
 * Danuar Reliquary (301330000): the second map, with its own treasure box.
 * <p>
 * Java: data/handlers/instance/DanuarReliquaryInstance_L.java, @InstanceID(301330000).
 *
 * @author Cheatkiller, Estrayl
 */
class DanuarReliquaryInstance_L : public DanuarReliquaryInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit DanuarReliquaryInstance_L(WorldMapInstance& instance);

public:
	static runtime::Ref<DanuarReliquaryInstance_L> create(WorldMapInstance& instance);

protected:
	int32_t getExitId() override;

	int32_t getTreasureBoxId() override;
};

} // namespace aion::gameserver::handlers::instance
