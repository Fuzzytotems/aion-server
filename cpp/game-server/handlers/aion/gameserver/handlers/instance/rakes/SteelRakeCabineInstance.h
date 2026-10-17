#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

namespace aion::gameserver::handlers::instance::rakes {

/**
 * Steel Rake Cabin (300460000): Sweeper Nunukin (219026) at one of two places; Madame Bovariki (219032) and the Steel Rake Shaman (219003) on
 * their two places in a random order.
 * <p>
 * Java: data/handlers/instance/rakes/SteelRakeCabineInstance.java.
 *
 * @author xTz
 */
class SteelRakeCabineInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit SteelRakeCabineInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<SteelRakeCabineInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;
};

} // namespace aion::gameserver::handlers::instance::rakes
