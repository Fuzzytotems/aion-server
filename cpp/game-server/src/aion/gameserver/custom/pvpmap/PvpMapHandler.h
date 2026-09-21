#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/custom/pvpmap/fwd.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::custom::pvpmap {

/**
 * The instance handler of the custom PvP map (301220000): join/leave teleportation, respawn locations, supply, keymaster, treasure chest and
 * random boss spawns.
 * <p>
 * Declaration header (docs/design/hub-headers.md §3.5, m5a-plan.md W-03): created by PvpMapService.init through InstanceService's handler
 * supplier (no @InstanceID marker; `create(instance)` stands for `PvpMapHandler::new`). RefCounted through GeneralInstanceHandler. The members
 * are the `fieldmap.py --class` block; the constructor and the accessors PvpMapService calls are ported, every other body is AION_UNPORTED
 * (the map is created only by PvpMapService.init, which is an AION_PARTIAL at M5a). The anonymous ItemUseObserver of getAllObserver and the
 * stored lambdas are defined in the .cpp when their bodies are ported.
 *
 * @author Yeats
 */
// gameserver::instance, because the sibling namespace aion::gameserver::custom::instance would shadow `instance` here
class PvpMapHandler : public gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t SHUGO_SPAWN_RATE = 30;
	// fieldmap: a static final int[] with literal contents is a static constexpr std::array (hub-headers.md §11.1)
	static constexpr std::array<int32_t, 21> RANDOM_BOSS_NPC_IDS{231196, 233740, 235759, 235765, 235763, 235767, 235771, 235619, 235620, 235621, 855822,
		855843, 230857, 230858, 297189, 855776, 219933, 219934, 235975, 855263, 231304};
	runtime::HashMap<int32_t, runtime::Ref<world::WorldPosition>> origins{AION_LOCK_CLASS(PvpMapHandler::origins)};
	runtime::HashMap<int32_t, int64_t> joinOrLeaveTime{AION_LOCK_CLASS(PvpMapHandler::joinOrLeaveTime)};
	runtime::HashMap<model::Race, runtime::Ref<runtime::RcArrayList<runtime::Ref<world::WorldPosition>>>> respawnLocations{
		AION_LOCK_CLASS(PvpMapHandler::respawnLocations)};
	runtime::ArrayList<runtime::Ref<world::WorldPosition>> treasurePositions{AION_LOCK_CLASS(PvpMapHandler::treasurePositions)};
	runtime::ArrayList<runtime::Ref<world::WorldPosition>> supplyPositions{AION_LOCK_CLASS(PvpMapHandler::supplyPositions)};
	runtime::ArrayList<runtime::Ref<world::WorldPosition>> keymasterPositions{AION_LOCK_CLASS(PvpMapHandler::keymasterPositions)};
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(PvpMapHandler::tasks)};
	runtime::Field<runtime::FutureRef> supplyTask{};
	runtime::Field<runtime::FutureRef> despawnTask{};
	runtime::Field<int32_t> currentRandomBossObjId{};

protected:
	explicit PvpMapHandler(world::WorldMapInstance& instance);
	~PvpMapHandler() override;

public:
	/** Java: new PvpMapHandler(instance) (the handler supplier PvpMapHandler::new) */
	static runtime::Ref<PvpMapHandler> create(world::WorldMapInstance& instance);

	void onInstanceCreate() override;

private:
	void spawnShugo(model::gameobjects::player::Player& player);

	void startSupplyTask();

	void scheduleSupplySpawn();

	void spawnKeymasters();

	void spawnKeymasterOrTreasureChest(int32_t npcId, bool isKeymaster);

	void scheduleRespawn(int32_t npcId, int32_t time, bool isKeymaster);

	void spawnTreasureChests();

	void startRandomBossTask();

	void scheduleRandomBossDespawn();

	void scheduleSupplyDespawn();

public:
	void join(model::gameobjects::player::Player& p);

	void leave(model::gameobjects::player::Player& p);

private:
	void startTeleportation(model::gameobjects::player::Player& p, bool isLeaving);

	/** Java: an anonymous ItemUseObserver (fieldmap PvpMapHandler_ItemUseObserver, defined in the .cpp when ported) */
	runtime::Ref<controllers::observer::ActionObserver> getAllObserver(model::gameobjects::player::Player& p);

	bool canJoin(model::gameobjects::player::Player& p);

	bool checkState(model::gameobjects::player::Player& p);

	void updateOrigin(model::gameobjects::player::Player& p); // synchronized

	void updateJoinOrLeaveTime(model::gameobjects::player::Player& p); // synchronized

public:
	bool onReviveEvent(model::gameobjects::player::Player& player) override;

	bool onDie(model::gameobjects::player::Player& player, model::gameobjects::Creature& lastAttacker) override;

	void onDie(model::gameobjects::Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Npc& npc) override;

private:
	void announceDeath(model::gameobjects::player::Player& player);

public:
	void onEnterInstance(model::gameobjects::player::Player& player) override;

	void onLeaveInstance(model::gameobjects::player::Player& player) override;

	void onPlayerLogout(model::gameobjects::player::Player& player) override;

	void onInstanceDestroy() override;

private:
	void cancelTasks();

	bool spawnAllowed();

public:
	int32_t getParticipantsSize();

private:
	void removePlayer(model::gameobjects::player::Player& p); // synchronized

	void revive(model::gameobjects::player::Player& player);

public:
	bool isAtVulnerableFortress(int32_t worldId, float x, float y, float z);

	bool isOnMap(model::gameobjects::Creature& creature);

	bool isRandomBoss(int32_t objectId);

	bool isRandomBossAlive();

private:
	void spawnNpcs();

	void addRespawnLocations();

	void addSupplyPositions();

	void addKeymasterPositions();

	void addTreasurePositions();

	std::string getZoneNameL10n(model::gameobjects::player::Player& player);

	int32_t getZoneNameL10nId(std::string_view zoneName);

public:
	float getApMultiplier() override;
};

} // namespace aion::gameserver::custom::pvpmap
