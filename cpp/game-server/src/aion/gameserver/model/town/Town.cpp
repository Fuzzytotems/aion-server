#include "aion/gameserver/model/town/Town.h"

#include <chrono>
#include <unordered_map>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/model/templates/spawns/housing/TownSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TOWNS_LIST.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::model::town {

using gameobjects::Persistable;

Town::Town(int32_t value, int32_t levelValue, int32_t pointsValue, Race raceValue, std::optional<commons::database::Timestamp> levelUpDateValue)
	// Java keeps the Timestamp the DAO read, which is null for a NULL towns.level_up_date; the member is a plain Timestamp (header request
	// m5h-t1), so a null column becomes the epoch here. towns.level_up_date is NOT NULL (sql/aion_gs.sql), so Java never stores null
	// (header-requests.md 5a-pre-5); docs/deviations/P5-11.md records the substitution
	: id(value), level(levelValue), points(pointsValue), levelUpDate(levelUpDateValue.value_or(commons::database::Timestamp{})), race(raceValue),
	  persistentState(Persistable::PersistentState::UPDATED) {
	// Java: this.spawnedNpcs = new ArrayList<>() is the member initializer
	spawnNewObjects();
	world::geo::GeoService::getInstance().updateTown(this->race, this->id, this->level.get());
}

runtime::Ref<Town> Town::create(int32_t value, int32_t levelValue, int32_t pointsValue, Race raceValue,
	std::optional<commons::database::Timestamp> levelUpDateValue) {
	return runtime::makeRef<Town>(value, levelValue, pointsValue, raceValue, levelUpDateValue);
}

Town::Town(int32_t value, Race raceValue) : Town(value, 1, 0, raceValue, commons::database::Timestamp(std::chrono::milliseconds(60000))) {
	this->persistentState.set(Persistable::PersistentState::NEW);
}

runtime::Ref<Town> Town::create(int32_t value, Race raceValue) {
	return runtime::makeRef<Town>(value, raceValue);
}

int32_t Town::getL10nId() const {
	int32_t idOffset = id - (race == Race::ELYOS ? 1001 : 2001);
	return (race == Race::ELYOS ? 403330 : 403360) + idOffset;
}

void Town::increasePoints(int32_t amount) {
	SYNCHRONIZED(*this) {
		switch (this->level.get()) {
			case 1:
				if (this->points.get() + amount >= 1000)
					increaseLevel();
				break;
			case 2:
				if (this->points.get() + amount >= 2000)
					increaseLevel();
				break;
			case 3:
				if (this->points.get() + amount >= 3000)
					increaseLevel();
				break;
			case 4:
				if (this->points.get() + amount >= 4000)
					increaseLevel();
				break;
		}
		this->points.set(this->points.get() + amount);
		setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED);
	}
}

// Java Town.java:109-116 (called from the synchronized increasePoints)
void Town::increaseLevel() {
	this->level.set(this->level.get() + 1);
	this->levelUpDate.set(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()))); // levelUpDate.setTime(now)
	broadcastUpdate();
	despawnOldObjects();
	spawnNewObjects();
	world::geo::GeoService::getInstance().updateTown(this->race, this->id, this->level.get());
}

// Java Town.java:118-131. The anonymous Consumer of forEachPlayer (Town$1, storage: sync) is the lambda: forEachPlayer runs it before it
// returns, so it captures the town and the packet by reference
void Town::broadcastUpdate() {
	std::unordered_map<int32_t, runtime::Ptr<Town>> data; // Java: new HashMap<>(1)
	data.emplace(this->id, runtime::Ptr<Town>(*this));
	network::aion::serverpackets::SM_TOWNS_LIST packet(data);
	world::World::getInstance().forEachPlayer([this, &packet](gameobjects::player::Player& player) {
		if (player.getRace() == race)
			utils::PacketSendUtility::sendPacket(player, packet);
	});
}

void Town::spawnNewObjects() {
	const auto* newSpawns = dataholders::DataManager::TOWN_SPAWNS_DATA->getSpawns(id, level.get());
	int32_t worldId = dataholders::DataManager::TOWN_SPAWNS_DATA->getWorldIdForTown(id);
	if (newSpawns == nullptr) // Java: NullPointerException in the for loop
		throw runtime::NullPointerException("Cannot iterate the spawns of town " + std::to_string(id) + ": no town spawn map has the town");
	for (const std::unique_ptr<templates::spawns::Spawn>& spawn : *newSpawns) {
		runtime::Ref<templates::spawns::SpawnGroup> spawnGroup = templates::spawns::SpawnGroup::create(worldId, spawn.get());
		for (const templates::spawns::SpawnSpotTemplate& sst : spawn->getSpawnSpotTemplates()) {
			// Java creates the TownSpawnTemplate for the group without adding it to its spots (SpawnGroup class comment)
			templates::spawns::SpawnTemplate& townSpawn = spawnGroup->adoptDetachedTemplate(
				std::make_unique<templates::spawns::housing::TownSpawnTemplate>(*spawnGroup, &sst, id));
			runtime::Ptr<gameobjects::VisibleObject> object = spawnengine::SpawnEngine::spawnObject(townSpawn, 1);
			// Java: (Npc) cast, a ClassCastException for another type, null is added as null
			spawnedNpcs.add(runtime::Ref<gameobjects::Npc>(runtime::cast<gameobjects::Npc>(object)));
		}
	}
}

void Town::despawnOldObjects() {
	for (runtime::Ptr<gameobjects::Npc> npc : spawnedNpcs.snapshot())
		npc->getController().delete_();
	spawnedNpcs.clear();
}

void Town::setPersistentState(gameobjects::Persistable::PersistentState state) {
	if (this->persistentState.get() != Persistable::PersistentState::NEW || state != Persistable::PersistentState::UPDATE_REQUIRED)
		this->persistentState.set(state);
}

Town::~Town() = default;

} // namespace aion::gameserver::model::town
