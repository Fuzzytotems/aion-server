#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services {

/**
 * 3.0 siege update (<a href="https://docs.google.com/document/d/1HVOw8-w9AlRp4ci0ei4iAzNaSKzAHj_xORu-qIQJFmc/edit#">3.0 Siege Docs</a>)
 *
 * @author SoulKeeper, Source, Neon, Estrayl
 */
class SiegeService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<siege::Siege>> activeSieges{AION_LOCK_CLASS(SiegeService::activeSieges#stripe)};
	runtime::AtomicBoolean isInitialized{AION_LOCK_CLASS(SiegeService::isInitialized)}; // Java: = new AtomicBoolean()
	runtime::HashMap<int32_t, runtime::Ref<model::siege::ArtifactLocation>> artifacts{AION_LOCK_CLASS(SiegeService::artifacts)};
	runtime::HashMap<int32_t, runtime::Ref<model::siege::FortressLocation>> fortresses{AION_LOCK_CLASS(SiegeService::fortresses)};
	runtime::HashMap<int32_t, runtime::Ref<model::siege::OutpostLocation>> outposts{AION_LOCK_CLASS(SiegeService::outposts)};
	runtime::HashMap<int32_t, runtime::Ref<model::siege::SiegeLocation>> locations{AION_LOCK_CLASS(SiegeService::locations)};
	const runtime::Ref<model::siege::AgentLocation> agent{};
	// fieldmap.toml: java.util.Date, null while the siege service is disabled (SiegeService.java:340), hub-headers.md §6
	runtime::Field<std::optional<commons::database::Timestamp>> nextStateUpdateTime{};
	runtime::Field<runtime::Ref<runtime::RcHashSet<runtime::Ref<model::gameobjects::player::Player>>>> rvrEventPlayers{}; // Java: = new HashSet<>()
	/**
	 * We should broadcast fortress status every hour Actually only an influence packet must be sent, but that doesn't matter
	 * <p>
	 * C++: Java CronExpressions.getOrCreate("0 0 * ? * *"); the immutable expression is held by value (hub-headers.md §6).
	 */
	static const cron::CronExpression SIEGE_LOCATION_STATUS_BROADCAST_SCHEDULE;
public:
	static SiegeService& getInstance(); // Java singleton
private:
	SiegeService();
	void updateNextStateUpdateTime();
public:
	void initSieges();
	void checkSiegeStart(int32_t locationId);
private:
	void startPreparations(int32_t locationId);
public:
	void startSiege(int32_t siegeLocationId); // synchronized
	void stopSiege(int32_t siegeLocationId); // synchronized
	/** Used to capture fortresses or artifacts without regular siege */
	void captureSiege(model::siege::SiegeRace sr, int32_t legionId, int32_t locId); // synchronized
private:
	void resetSiegeLocation(model::siege::SiegeLocation& loc); // synchronized
	/** Updates next state for fortresses */
	void updateFortressNextState();
	std::unordered_map<int32_t, commons::database::Timestamp> collectNextSiegeStartDates();
public:
	int32_t getSecondsUntilNextFortressState();
	int32_t getRemainingSiegeTimeInSeconds(int32_t siegeLocationId);
	runtime::Ptr<siege::Siege> getSiege(model::siege::SiegeLocation& loc);
	runtime::Ptr<siege::Siege> getSiege(int32_t siegeLocationId);
	bool isSiegeInProgress(int32_t fortressId);
	runtime::HashMap<int32_t, runtime::Ref<model::siege::OutpostLocation>>& getOutposts() { return this->outposts; }
	runtime::Ptr<model::siege::OutpostLocation> getOutpost(int32_t id);
	runtime::HashMap<int32_t, runtime::Ref<model::siege::FortressLocation>>& getFortresses() { return this->fortresses; }
	runtime::Ptr<model::siege::FortressLocation> getFortress(int32_t id);
	runtime::HashMap<int32_t, runtime::Ref<model::siege::ArtifactLocation>>& getArtifacts() { return this->artifacts; }
	runtime::Ptr<model::siege::ArtifactLocation> getArtifact(int32_t id);
	std::vector<runtime::Ptr<model::siege::ArtifactLocation>> getStandaloneArtifacts();
	runtime::Ptr<model::siege::ArtifactLocation> getFortressArtifact(int32_t siegeLocId);
	const model::templates::siegelocation::DoorRepairData* getDoorRepairData(int32_t siegeId);
	const model::templates::siegelocation::DoorRepairStone* getRepairStone(int32_t siegeId, int32_t repairStoneStaticId);
	runtime::HashMap<int32_t, runtime::Ref<model::siege::SiegeLocation>>& getSiegeLocations() { return this->locations; }
	runtime::Ptr<model::siege::SiegeLocation> getSiegeLocation(int32_t id);
	/** C++: Java LinkedHashMap filled in the iteration order of `locations`; a std::map keeps the ids ordered */
	std::map<int32_t, runtime::Ptr<model::siege::SiegeLocation>> getSiegeLocations(int32_t worldId);
	runtime::Ptr<model::siege::AgentLocation> getAgentLocation() const { return this->agent; }
private:
	/** @return the newly created per-run siege (Java `new FortressSiege(...)` etc.), hub-headers.md §5 */
	runtime::Ref<siege::Siege> newSiege(int32_t siegeLocationId);
public:
	void cleanLegionId(int32_t legionId);
	void updateOutpostSiegeState(model::siege::FortressLocation& fortressLoc); // synchronized
	void spawnNpcs(int32_t siegeLocationId, model::siege::SiegeRace race, model::siege::SiegeModType type);
	void deSpawnNpcs(int32_t siegeLocationId);
	bool isRespawnAllowed(model::gameobjects::Npc& npc);
	void broadcastUpdate(model::siege::SiegeLocation& loc);
	void broadcastStatusAndUpdate(model::siege::OutpostLocation& outpost, bool oldSilentraState);
private:
	void broadcast(network::aion::serverpackets::SM_RIFT_ANNOUNCE& rift, network::aion::serverpackets::SM_SYSTEM_MESSAGE& info);
public:
	runtime::Ptr<model::siege::FortressLocation> findFortress(int32_t worldId, float x, float y, float z);
	void onPlayerLogin(model::gameobjects::player::Player& player);
	void onEnterSiegeWorld(model::gameobjects::player::Player& player);
	void onAbyssPointsAdded(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& obj, int32_t abyssPoints);
	int32_t getSiegeIdByLocId(int32_t locId);
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::gameobjects::player::Player>>> getRvrEventPlayers() const {
		return this->rvrEventPlayers.get();
	}
	/** Checks if the player is in an RVR event list, if not the player is added. */
	void checkRvrEventPlayer(runtime::Ptr<model::gameobjects::player::Player> player);
	void clearRvrEventPlayers();
private:
	/**
	 * Modifies to original cron expression to add additional time for preparations.
	 * Five minutes for regular fortress sieges.
	 * Ten minutes for Panesterra fortress sieges.
	 */
	std::string getPreparationCronString(std::string_view siegeTime, int32_t fortressId);
};

} // namespace aion::gameserver::services
