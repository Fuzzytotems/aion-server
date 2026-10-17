#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

namespace aion::gameserver::handlers::instance::rakes {

/**
 * Steel Rake (300100000): Pegureronerk (798376) three times in four, else the Kaneron Agent (215041), at the same place; one of six special
 * deliveries (215074-215077, 215054, 215055).
 * <p>
 * Java: data/handlers/instance/rakes/SteelRakeInstance.java.
 *
 * @author xTz
 */
class SteelRakeInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit SteelRakeInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<SteelRakeInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;
};

} // namespace aion::gameserver::handlers::instance::rakes
