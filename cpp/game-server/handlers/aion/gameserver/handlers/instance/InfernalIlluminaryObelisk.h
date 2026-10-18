#pragma once

#include "aion/gameserver/handlers/instance/IlluminaryObeliskInstance.h"

#include <cstdint>

namespace aion::gameserver::handlers::instance {

/**
 * Infernal Illuminary Obelisk (301370000): the Illuminary Obelisk with the infernal waves and Infernal Dynatoum (234686).
 * <p>
 * Java: data/handlers/instance/InfernalIlluminaryObelisk.java, @InstanceID(301370000) (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class InfernalIlluminaryObelisk : public IlluminaryObeliskInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit InfernalIlluminaryObelisk(WorldMapInstance& instance);

public:
	static runtime::Ref<InfernalIlluminaryObelisk> create(WorldMapInstance& instance);

protected:
	void spawnEndboss(int32_t npcId) override;

	void scheduleChargeAttacks(int32_t npcId) override;
};

} // namespace aion::gameserver::handlers::instance
