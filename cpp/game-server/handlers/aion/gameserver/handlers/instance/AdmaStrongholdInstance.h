#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

namespace aion::gameserver::handlers::instance {

/**
 * Adma Stronghold (320130000): a random Lannok treasure chest keeper at one of three places, half of the time.
 * <p>
 * Java: data/handlers/instance/AdmaStrongholdInstance.java.
 *
 * @author xTz, Tibald
 */
class AdmaStrongholdInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit AdmaStrongholdInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<AdmaStrongholdInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;
};

} // namespace aion::gameserver::handlers::instance
