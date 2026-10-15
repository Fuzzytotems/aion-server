#pragma once

#include "aion/gameserver/handlers/instance/dredgion/DredgionInstance.h"

namespace aion::gameserver::handlers::instance::dredgion {

/**
 * Baranath Dredgion (300110000): a named npc at one of two places at random, the Surkanas whose rooms the races capture, the boss after
 * five of them, the hatches and gates.
 * <p>
 * Java: data/handlers/instance/dredgion/BaranathDredgionInstance.java, @InstanceID(300110000).
 *
 * @author xTz
 */
class BaranathDredgionInstance : public DredgionInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit BaranathDredgionInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<BaranathDredgionInstance> create(WorldMapInstance& instance);

	using DredgionInstance::onDie;

	void onEnterInstance(Player& player) override;

private:
	void onDieSurkana(Npc& npc, Player& mostPlayerDamage, int32_t points);

public:
	void onDie(Npc& npc) override;

	void onInstanceCreate() override;

protected:
	void openFirstDoors() override;
};

} // namespace aion::gameserver::handlers::instance::dredgion
