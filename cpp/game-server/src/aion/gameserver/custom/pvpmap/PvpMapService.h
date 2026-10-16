#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/custom/pvpmap/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::custom::pvpmap {

/**
 * Creates the custom PvP map instance at startup and forwards join, leave and boss queries to its handler.
 * <p>
 * An Immortal singleton (hub-headers.md §11.2; Java's static final instance). `handler` stays null until init() creates the map: at M5a
 * init() is an AION_PARTIAL (InstanceService.getNextAvailableInstance is not ported, m5a-plan.md W-03), so onLogin returns at its null check
 * like Java before init (PlayerEnterWorldService.java:384) and isOnPvPMap is false.
 *
 * @author Yeats
 */
class PvpMapService : public runtime::Immortal {
private:
	runtime::Field<runtime::Ref<PvpMapHandler>> handler{};

	PvpMapService();
	~PvpMapService();

public:
	static PvpMapService& getInstance();

	void init();

	void onLogin(model::gameobjects::player::Player& player);

	void notifyBossSpawn(model::gameobjects::player::Player& player);

	bool isRandomBoss(model::gameobjects::Npc& npc);

	void joinMap(model::gameobjects::player::Player& p);

	void leaveMap(model::gameobjects::player::Player& p);

	bool isOnPvPMap(model::gameobjects::Creature& creature);

	int32_t getParticipantsSize();

	/** Java: package-private, called by PvpMapHandler.onInstanceDestroy */
	void onInstanceDestroy();
};

} // namespace aion::gameserver::custom::pvpmap
