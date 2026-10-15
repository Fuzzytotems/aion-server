#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

namespace aion::gameserver::handlers::instance {

/**
 * Haramel (300200000): Hamerun's death opens the gate and brings the treasure chest of the class of the player who did the most damage;
 * the miners and the cart vanish when they die.
 * <p>
 * Java: data/handlers/instance/HaramelInstance.java.
 *
 * @author Undertrey
 */
class HaramelInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit HaramelInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<HaramelInstance> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
