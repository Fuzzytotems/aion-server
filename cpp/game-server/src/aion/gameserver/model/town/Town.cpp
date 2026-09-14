#include "aion/gameserver/model/town/Town.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::model::town {

Town::Town(int32_t value, int32_t levelValue, int32_t pointsValue, Race raceValue, std::optional<commons::database::Timestamp> levelUpDateValue)
	: id(value), level(levelValue), points(pointsValue), levelUpDate(), race(raceValue) {
	// Java: this.levelUpDate = levelUpDate; this.persistentState = PersistentState.UPDATED; this.spawnedNpcs = new ArrayList<>(); spawnNewObjects();
	// GeoService.getInstance().updateTown(this.race, this.id, this.level)
	AION_UNPORTED();
}

runtime::Ref<Town> Town::create(int32_t value, int32_t levelValue, int32_t pointsValue, Race raceValue,
	std::optional<commons::database::Timestamp> levelUpDateValue) {
	return runtime::makeRef<Town>(value, levelValue, pointsValue, raceValue, levelUpDateValue);
}

Town::Town(int32_t value, Race raceValue)
	: id(), levelUpDate(), race() {
	// Java: this(id, 1, 0, race, new Timestamp(60000)); this.persistentState = PersistentState.NEW
	AION_UNPORTED();
}

runtime::Ref<Town> Town::create(int32_t value, Race raceValue) {
	return runtime::makeRef<Town>(value, raceValue);
}

int32_t Town::getL10nId() const {
	AION_UNPORTED();
}

void Town::increasePoints(int32_t amount) {
	AION_UNPORTED();
}

void Town::increaseLevel() {
	AION_UNPORTED();
}

// anonymous Consumer at Town.java:122 (model.town.Town$1); argument 1 of forEachPlayer(); storage: sync
void Town::broadcastUpdate() {
	AION_UNPORTED();
}

void Town::spawnNewObjects() {
	AION_UNPORTED();
}

void Town::despawnOldObjects() {
	AION_UNPORTED();
}

void Town::setPersistentState(gameobjects::Persistable::PersistentState state) {
	AION_UNPORTED();
}

Town::~Town() = default;

} // namespace aion::gameserver::model::town
