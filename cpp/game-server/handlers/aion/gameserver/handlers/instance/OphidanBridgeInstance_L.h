#pragma once

#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"

namespace aion::gameserver::handlers::instance {

/**
 * Ophidan Bridge (301320000): the second map of the same instance.
 * <p>
 * Java: data/handlers/instance/OphidanBridgeInstance_L.java, @InstanceID(301320000).
 *
 * @author Cheatkiller
 */
class OphidanBridgeInstance_L : public OphidanBridgeInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit OphidanBridgeInstance_L(WorldMapInstance& instance);

public:
	static runtime::Ref<OphidanBridgeInstance_L> create(WorldMapInstance& instance);
};

} // namespace aion::gameserver::handlers::instance
