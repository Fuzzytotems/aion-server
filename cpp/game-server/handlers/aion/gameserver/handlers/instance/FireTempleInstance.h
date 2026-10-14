#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

namespace aion::gameserver::handlers::instance {

/**
 * Fire Temple (320100000): each named boss spawns at 75 %, an elite in its place otherwise; the stronger Kromede at 10 %.
 * <p>
 * Java: data/handlers/instance/FireTempleInstance.java.
 *
 * @author Gigi
 */
class FireTempleInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit FireTempleInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<FireTempleInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;
};

} // namespace aion::gameserver::handlers::instance
