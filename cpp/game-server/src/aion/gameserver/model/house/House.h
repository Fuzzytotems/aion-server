#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::house {

/**
 * Hub header (docs/design/hub-headers.md). Java `new House(...)` is `VisibleObject::create<House>(...)`. The constructors store the members;
 * postConstruct() runs the Java constructor body in Java order: `getController().setOwner(*this)`, the PlayerAwareKnownList part,
 * resetDoorState() and setPersistentState(UPDATED).
 * The acquired time and the next payment are nullable timestamps in Java (HousingService.java:112): `std::optional`. The owner name is null
 * for houses without an owner.
 *
 * @author Rolandas
 */
class House : public gameobjects::VisibleObject, public gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	const templates::housing::HouseAddress* address;
	runtime::Field<const templates::housing::Building*> building{};
	runtime::Field<int32_t> ownerId{};
	runtime::Field<std::optional<commons::database::Timestamp>> acquiredTime{};
	runtime::Field<HouseDoorState> doorState{};
	runtime::Field<bool> showOwnerName{true};
	runtime::Field<bool> inactive{};
	runtime::Field<std::optional<commons::database::Timestamp>> nextPay{};
	runtime::Field<runtime::Ref<HouseBids>> bids{};
	runtime::EnumMap<templates::spawns::SpawnType, runtime::Ref<gameobjects::Npc>> spawns{AION_LOCK_CLASS(House::spawns)};
	runtime::Field<runtime::Ref<HouseRegistry>> houseRegistry{};
	runtime::Field<runtime::Ref<gameobjects::player::PlayerScripts>> playerScripts{};
	runtime::Field<gameobjects::Persistable::PersistentState> persistentState{};
	runtime::Field<std::string> signNotice{};

protected:
	/** Java: this(IDFactory.getInstance().nextId(), address.getLand().getDefaultBuilding(), address, instanceId) */
	House(CreateKey key, const templates::housing::HouseAddress* address, int32_t instanceId);

	House(CreateKey key, int32_t objectId, const templates::housing::Building* building, const templates::housing::HouseAddress* address,
		int32_t instanceId);

	~House() override;

	/** Java constructor body: getController().setOwner(this), setKnownlist(new PlayerAwareKnownList(this)), resetDoorState(), UPDATED */
	void postConstruct() override;

public:
	/** Narrows VisibleObject::getController (Java cast-only override) */
	controllers::HouseController& getController() const;

	std::string getName() override;

	const templates::housing::HouseAddress* getAddress() const { return address; }

	const templates::housing::HousingLand* getLand();

	world::WorldType getWorldType() override;

	const templates::housing::Building* getBuilding() const { return building.get(); }

	void setBuilding(const templates::housing::Building* building);

	float getVisibleDistance() override;

	int32_t getOwnerId() const { return ownerId.get(); }

	void setOwnerId(int32_t ownerId);

	/** @return the owner name, std::nullopt without an owner */
	std::optional<std::string> getOwnerName();

	std::optional<commons::database::Timestamp> getAcquiredTime() const { return acquiredTime.get(); }

	void setAcquiredTime(std::optional<commons::database::Timestamp> acquiredTime);

	int32_t getPermissionsForDB();

	void setPermissionsFromDB(int32_t permissions);

	HouseDoorState getDoorState() const { return doorState.get(); }

	bool resetDoorState();

	bool setDoorState(HouseDoorState doorState);

	/** @return True if the owner name should be displayed in the house sign tooltip */
	bool isShowOwnerName() const { return showOwnerName.get(); }

	void setShowOwnerName(bool showOwnerName);

	/** @return True if the owner of this house has another (newly acquired) house */
	bool isInactive() const { return inactive.get(); }

	void setInactive(bool value) { inactive.set(value); }

	bool isFeePaid();

	std::optional<commons::database::Timestamp> getNextPay() const { return nextPay.get(); }

	/** Java takes a java.util.Date (null clears the payment date) */
	void setNextPay(std::optional<commons::database::Timestamp> nextPay);

	runtime::Ptr<HouseBids> getBids() const { return bids.get(); }

	void setBids(runtime::Ptr<HouseBids> bids, bool resetDoorStateOfUnoccupiedHouse);

	runtime::Ptr<gameobjects::Npc> getButler();

	runtime::Ptr<gameobjects::Npc> getRelationshipCrystal();

	runtime::Ptr<gameobjects::Npc> getCurrentSign();

private:
	runtime::Ptr<gameobjects::Npc> getSpawn(templates::spawns::SpawnType type);

public:
	/** C++: keeps VisibleObject::getSpawn() (the spawn template) callable on House next to the private overload (no hiding in Java) */
	using gameobjects::VisibleObject::getSpawn;

	/** @param npc the new spawn, null removes the old one (HouseController.java:173) */
	void updateSpawn(templates::spawns::SpawnType type, runtime::Ptr<gameobjects::Npc> npc);

	/** Do not use directly !!! It's for instance destroy of studios only. Studios get reused, Npcs are despawned by instance destroy */
	void clearSpawns();

	/** @return the registry (loaded lazily) */
	runtime::Ptr<HouseRegistry> getRegistry();

	// synchronized
	void resetRegistry();

	// synchronized
	void reloadHouseRegistry();

	/** @return the player scripts (loaded lazily) */
	runtime::Ptr<gameobjects::player::PlayerScripts> getPlayerScripts();

	// synchronized
	void reloadPlayerScripts();

	templates::housing::HouseType getHouseType();

	// synchronized
	void save();

	PersistentState getPersistentState() override { return persistentState.get(); }

	void setPersistentState(PersistentState persistentState) override;

	int8_t getHouseOwnerStates();

	std::string getSignNotice() const { return signNotice.get(); }

	void setSignNotice(std::string_view notice);

	bool canEnter(gameobjects::player::Player& player);

	/** Java final */
	int64_t getDefaultAuctionPrice();

	/** @return Calculated heading for a player inside looking towards the wall where butler, relationship crystal and the door are located. */
	int8_t getTeleportHeading();

	int32_t getTownLevel();

	/**
	 * @return Seconds until this inactive house will be activated and the old one gets removed from the owner. Returns -1 if this house is already
	 *         active.
	 */
	int32_t secondsUntilGraceEnd();

private:
	/** Grace end happens on auction end, so we find the nearest auction end date taking place around two weeks after the house was bought. */
	commons::database::Timestamp findGraceEndTime();

public:
	bool matchesLandRace(Race race);

	void sendScripts(gameobjects::player::Player& player);
};

} // namespace aion::gameserver::model::house
