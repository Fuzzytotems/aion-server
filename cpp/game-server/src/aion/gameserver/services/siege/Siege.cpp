#include "aion/gameserver/services/siege/Siege.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"

namespace aion::gameserver::services::siege {

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

Siege::Siege(model::siege::SiegeLocation& value)
	: siegeCounter(SiegeCounter::create()), siegeLocation(runtime::Ref<model::siege::SiegeLocation>(value)) {
}

void Siege::startSiege() {
	AION_UNPORTED();
}

void Siege::startSiege(int32_t locationId) {
	AION_UNPORTED();
}

void Siege::stopSiege() {
	AION_UNPORTED();
}

int32_t Siege::getSiegeLocationId() {
	AION_UNPORTED();
}

void Siege::setBoss(runtime::Ptr<model::gameobjects::siege::SiegeNpc> value) {
	this->boss.set(value);
}

runtime::Ptr<SiegeRaceCounter> Siege::getWinnerRaceCounter() {
	AION_UNPORTED();
}

bool Siege::isFinished() {
	AION_UNPORTED();
}

void Siege::initSiegeBoss() {
	AION_UNPORTED();
}

void Siege::spawnNpcs(int32_t locationId, model::siege::SiegeRace race, model::siege::SiegeModType type) {
	AION_UNPORTED();
}

void Siege::despawnNpcs(int32_t locationId) {
	AION_UNPORTED();
}

void Siege::broadcastState(model::siege::SiegeLocation& location) {
	AION_UNPORTED();
}

void Siege::broadcastUpdate(model::siege::SiegeLocation& location) {
	AION_UNPORTED();
}

void Siege::updateOutpostStatusByFortress(model::siege::FortressLocation& location) {
	AION_UNPORTED();
}

void Siege::sendRewardsToParticipants(SiegeRaceCounter& raceCounter, mail::SiegeResult raceResult) {
	AION_UNPORTED();
}

std::string Siege::toString() {
	AION_UNPORTED();
}

Siege::~Siege() = default;

} // namespace aion::gameserver::services::siege
