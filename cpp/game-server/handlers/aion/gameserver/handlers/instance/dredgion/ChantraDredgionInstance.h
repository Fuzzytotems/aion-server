#pragma once

#include "aion/gameserver/handlers/instance/dredgion/DredgionInstance.h"

namespace aion::gameserver::handlers::instance::dredgion {

/**
 * Chantra Dredgion (300210000): the hatches after 12 minutes, the random named npcs, the Surkanas, the boss after five of them, a named
 * npc 10-15 minutes in.
 * <p>
 * Java: data/handlers/instance/dredgion/ChantraDredgionInstance.java, @InstanceID(300210000).
 *
 * @author xTz
 */
class ChantraDredgionInstance : public DredgionInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit ChantraDredgionInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<ChantraDredgionInstance> create(WorldMapInstance& instance);

	using DredgionInstance::onDie;

	void onEnterInstance(Player& player) override;

private:
	void onDieSurkana(Npc& npc, Player& mostPlayerDamage, int32_t points);

public:
	void onDie(Npc& npc) override;

protected:
	void openFirstDoors() override;
};

} // namespace aion::gameserver::handlers::instance::dredgion
