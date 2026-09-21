#include "aion/gameserver/model/siege/Influence.h"

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::model::siege {

using InfluencesByWorld = runtime::RcLinkedHashMap<int32_t, runtime::Ref<runtime::RcHashMap<SiegeRace, int32_t>>>;
using RaceInfluences = runtime::RcHashMap<SiegeRace, int32_t>;

Influence::Influence() {
	recalculateInfluence();
}

Influence::~Influence() = default;

Influence& Influence::getInstance() {
	static Influence instance; // Java: private static final Influence instance = new Influence()
	return instance;
}

void Influence::recalculateInfluence() {
	influencesByWorld.set(calculateFortressWorldInfluences());
	globalInfluences.set(calculateGlobalInfluences(*influencesByWorld.get()));
	elyosInfluenceRate.set(getInfluence(SiegeRace::ELYOS) / 100.0f);
	asmoInfluenceRate.set(getInfluence(SiegeRace::ASMODIANS) / 100.0f);
	balaurInfluenceRate.set(getInfluence(SiegeRace::BALAUR) / 100.0f);
}

runtime::Ref<InfluencesByWorld> Influence::calculateFortressWorldInfluences() {
	runtime::Ref<InfluencesByWorld> fortressWorldInfluences = InfluencesByWorld::create(AION_LOCK_CLASS(Influence::influencesByWorld));
	for (const runtime::Ptr<SiegeLocation>& sLoc : services::SiegeService::getInstance().getSiegeLocations().values()) {
		int32_t influence = sLoc->getInfluenceValue();
		if (influence > 0) {
			// Java: fortressWorldInfluences.compute(worldId, ...) creating the EnumMap on first use, then influences.compute(race, sum)
			runtime::Ptr<RaceInfluences> influences = fortressWorldInfluences->get(sLoc->getWorldId());
			if (!influences) {
				runtime::Ref<RaceInfluences> created = RaceInfluences::create(AION_LOCK_CLASS(Influence::influencesByWorld#value));
				influences = created;
				fortressWorldInfluences->put(sLoc->getWorldId(), std::move(created));
			}
			SiegeRace race = sLoc->getRace();
			influences->put(race, influences->getOrDefault(race, 0) + influence);
		}
	}
	return fortressWorldInfluences;
}

runtime::Ref<RaceInfluences> Influence::calculateGlobalInfluences(InfluencesByWorld& influencesByWorldValue) {
	// Java: Collections.emptyMap() when there are no influences, else the per race sums (EnumMap)
	runtime::Ref<RaceInfluences> sums = RaceInfluences::create(AION_LOCK_CLASS(Influence::globalInfluences));
	for (const runtime::Ptr<RaceInfluences>& influences : influencesByWorldValue.values()) {
		for (const auto& entry : influences->entrySet())
			sums->put(entry.getKey(), sums->getOrDefault(entry.getKey(), 0) + entry.getValue());
	}
	return sums;
}

int32_t Influence::getInfluence(SiegeRace race) {
	return globalInfluences.get()->getOrDefault(race, 0);
}

int32_t Influence::getInfluence(int32_t worldId, SiegeRace race) {
	runtime::Ptr<RaceInfluences> influences = influencesByWorld.get()->get(worldId);
	return influences ? influences->getOrDefault(race, 0) : 0;
}

std::vector<int32_t> Influence::getInfluenceRelevantWorldIds() {
	std::vector<int32_t> worldIds;
	for (int32_t worldId : influencesByWorld.get()->keySet())
		worldIds.push_back(worldId);
	return worldIds;
}

int32_t Influence::getPvpRaceBonusRatio(Race attRace) {
	switch (attRace) {
		case Race::ASMODIANS:
			return calculatePvpRaceBonusRatio(getAsmodianInfluenceRate(), getElyosInfluenceRate());
		case Race::ELYOS:
			return calculatePvpRaceBonusRatio(getElyosInfluenceRate(), getAsmodianInfluenceRate());
		default:
			return 0;
	}
}

int32_t Influence::calculatePvpRaceBonusRatio(float ownInfluence, float enemyInfluence) {
	if (enemyInfluence >= 0.81f && ownInfluence <= 0.10f)
		return 200;
	else if (enemyInfluence >= 0.81f || (enemyInfluence >= 0.71f && ownInfluence <= 0.10f))
		return 150;
	else if (enemyInfluence >= 0.71f)
		return 100;
	return 0;
}

} // namespace aion::gameserver::model::siege
