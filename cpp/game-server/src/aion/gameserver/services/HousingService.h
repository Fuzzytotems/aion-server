#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author Rolandas, Neon
 */
class HousingService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::house::House>> customHouses{AION_LOCK_CLASS(HousingService::customHouses#stripe)};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::house::House>> studios{AION_LOCK_CLASS(HousingService::studios#stripe)};
public:
	static HousingService& getInstance(); // Java singleton
private:
	HousingService();
	~HousingService();
	void revokeOwnershipOfDeletedPlayers();
	void updateInactiveStateForAllHouses();
	/**
	 * Only for server start. Doesn't send packets or reloads house registries because it should only be run once on init.
	 */
	void updateInactiveStateForPlayerHouses(int32_t playerObjId);
public:
	void changeOwner(model::house::House& house, int32_t newOwnerId);
private:
	void notifyAboutOwnerChange(int32_t ownerId, int32_t addressId, bool isNewOwner);
public:
	void spawnHouses(world::WorldMapInstance& instance, int32_t registeredId);
private:
	void spawnStudio(int32_t worldId, int32_t instanceId, int32_t registeredId);
public:
	std::vector<runtime::Ptr<model::house::House>> findPlayerHouses(int32_t playerObjId);
	runtime::Ptr<model::house::House> findActiveHouse(int32_t playerObjId);
	runtime::Ptr<model::house::House> findInactiveHouse(int32_t playerObjId);
	runtime::Ptr<model::house::House> getHouseByAddress(int32_t address);
	runtime::Ptr<model::house::House> getPlayerStudio(int32_t playerId);
	bool removeStudio(model::house::House& studio);
	void registerPlayerStudio(model::gameobjects::player::Player& player);
	void recreatePlayerStudio(model::gameobjects::player::Player& player);
private:
	void createStudio(model::gameobjects::player::Player& player, bool chargeFee);
public:
	bool canOwnHouse(model::gameobjects::player::Player& player, bool notify);
	void switchHouseBuilding(model::house::House& currentHouse, int32_t newBuildingId);
	std::vector<runtime::Ptr<model::house::House>> getCustomHouses();
	runtime::Ptr<model::house::House> findHouse(int32_t objId);
	runtime::Ptr<model::house::House> findStudio(int32_t objId);
	runtime::Ptr<model::house::House> findHouseOrStudio(int32_t objId);
	void onPlayerDeleted(int32_t playerObjId);
	void onPlayerLogin(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services
