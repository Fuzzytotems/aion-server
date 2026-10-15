#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::instance {

/**
 * Rentus Base (300280000, shortened in 4.8: it starts behind Zantaraz): the two lieutenants open their door together; Kuhara's death
 * turns him and switches the doors; Vasharti's death opens the exit, the supplies box and Ariana's farewell with the walking soldiers;
 * the race's teleport devices with their buffs; Xasta's walk and his outcome by his health; the bosses.
 * <p>
 * Java: data/handlers/instance/RentusBaseInstance.java, @InstanceID(300280000). C++: the scheduled lambdas are pinned to the handler.
 *
 * @author xTz, Estrayl
 */
class RentusBaseInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isRaceKnown{AION_LOCK_CLASS(RentusBaseInstance::isRaceKnown)}; // Java: = new AtomicBoolean()
	runtime::AtomicBoolean isXastaEventStarted{AION_LOCK_CLASS(RentusBaseInstance::isXastaEventStarted)}; // Java: = new AtomicBoolean()

protected:
	explicit RentusBaseInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<RentusBaseInstance> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;

	void onAggro(Npc& npc) override;

private:
	void spawnEndEvent(int32_t npcId, std::string_view walkern, int32_t time);

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time, std::string_view walkern);

	bool isDeadNpc(int32_t npcId);

public:
	void onCreatureDetected(Npc& detector, Creature& detected) override;

	void onSpecialEvent(Npc& npc) override;

	void onEnterInstance(Player& player) override;

	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
