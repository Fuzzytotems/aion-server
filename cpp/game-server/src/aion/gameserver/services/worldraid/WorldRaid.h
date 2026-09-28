#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/worldraid/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/worldraid/fwd.h"

namespace aion::gameserver::services::worldraid {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Whoop, Sykra
 */
class WorldRaid : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const model::templates::worldraid::WorldRaidLocation* raidLocation;
	const bool useSpecialSpawnMsg;
	const bool sendMessages;
	runtime::AtomicBoolean isFinished_{AION_LOCK_CLASS(WorldRaid::isFinished)}; // Java: = new AtomicBoolean()
	runtime::AtomicBoolean isStarted{AION_LOCK_CLASS(WorldRaid::isStarted)}; // Java: = new AtomicBoolean()
	runtime::Field<const model::templates::worldraid::WorldRaidNpc*> randomBossTemplate{};
	runtime::Field<runtime::Ref<model::gameobjects::Npc>> boss{};
	runtime::Field<runtime::Ref<model::gameobjects::Npc>> flag{};
	runtime::Field<runtime::Ref<model::gameobjects::Npc>> vortex{};
	runtime::ArrayList<runtime::Ref<model::gameobjects::Npc>> locationMarkers{AION_LOCK_CLASS(WorldRaid::locationMarkers)}; // Java: = new ArrayList<>()
	runtime::Field<runtime::FutureRef> stopRaidTask{};
	runtime::Field<runtime::FutureRef> preparationTask{};

protected:
	WorldRaid(const model::templates::worldraid::WorldRaidLocation* raidLocation, bool useSpecialSpawnMsg, bool sendMessages);

public:
	static runtime::Ref<WorldRaid> create(const model::templates::worldraid::WorldRaidLocation* value, bool useSpecialSpawnMsgValue,
		bool sendMessagesValue);

	void startWorldRaid();

	void stopWorldRaid();

private:
	void onWorldRaidStart();

	void onWorldRaidFinish();

	void scheduleBossDespawn();

	void cancelStopRaidTask();

	void despawnNpcs(std::initializer_list<runtime::Ptr<model::gameobjects::Npc>> npcs = {});

	void despawnNpcs(const std::vector<runtime::Ptr<model::gameobjects::Npc>>& npcs);

	void spawnAndInitRandomBoss();

	void registerDeathObserver(model::gameobjects::Npc& npc);

	void spawnAndInitMapFlag();

	void spawnAndInitVortex();

	void spawnAndInitMarkerSpots();

	void broadcastMessage(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg);

	void broadcastMessage(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg, bool forceMsg);

public:
	int32_t getLocationId();

	bool isFinished();

protected:
	~WorldRaid() override;
};

} // namespace aion::gameserver::services::worldraid
