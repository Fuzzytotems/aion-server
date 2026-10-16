#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::instance {

/**
 * Occupied Rentus Base (300620000): the harder Rentus Base - the same events with its own npcs, and the two devices' casts.
 * <p>
 * Java: data/handlers/instance/OccupiedRentusBaseInstance.java, @InstanceID(300620000). C++: the scheduled lambdas are pinned to the handler.
 *
 * @author Tibald, Estrayl
 */
class OccupiedRentusBaseInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isRaceKnown{AION_LOCK_CLASS(OccupiedRentusBaseInstance::isRaceKnown)}; // Java: = new AtomicBoolean()
	runtime::AtomicBoolean isXastaEventStarted{AION_LOCK_CLASS(OccupiedRentusBaseInstance::isXastaEventStarted)}; // Java: = new AtomicBoolean()

protected:
	explicit OccupiedRentusBaseInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<OccupiedRentusBaseInstance> create(WorldMapInstance& instance);

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
