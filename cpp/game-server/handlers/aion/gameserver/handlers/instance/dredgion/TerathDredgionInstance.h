#pragma once

#include "aion/gameserver/handlers/instance/dredgion/DredgionInstance.h"

namespace aion::gameserver::handlers::instance::dredgion {

/**
 * Terath Dredgion (300440000): the hatches after 12 minutes, the random named npcs (one walks), the Surkanas, Captain Anusa after five
 * of them, Enforcer Udara 10-15 minutes in, the custom medal rewards.
 * <p>
 * Java: data/handlers/instance/dredgion/TerathDredgionInstance.java, @InstanceID(300440000).
 *
 * @author xTz
 */
class TerathDredgionInstance : public DredgionInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit TerathDredgionInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<TerathDredgionInstance> create(WorldMapInstance& instance);

	using DredgionInstance::onDie;

	void onEnterInstance(Player& player) override;

private:
	void onDieSurkana(Npc& npc, Player& mostPlayerDamage, int32_t points);

public:
	void onDie(Npc& npc) override;

	void doReward(Player& player, PvpInstancePlayerReward& reward, Race winningRace) override;

protected:
	void openFirstDoors() override;
};

} // namespace aion::gameserver::handlers::instance::dredgion
