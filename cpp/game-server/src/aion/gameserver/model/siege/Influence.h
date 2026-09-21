#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"

namespace aion::gameserver::model::siege {

/**
 * Holds and calculates the faction influences based on the fortresses template influence values (which should sum up to 100).<br/>
 * The faction who owns a fortress gets the associated influence value. Based on these values, the global influence rate is then calculated.
 * <p>
 * C++: an Immortal singleton (hub-headers.md §11.2); the Java static instance is created on first use by getInstance(), after the static data
 * (Java: class initialization at the first getInstance() call). recalculateInfluence publishes new maps into the fields, like Java assigns new
 * collections. influencesByWorld keeps Java's LinkedHashMap order (SM_INFLUENCE_RATIO writes the world ids in that order); EnumMap values are
 * HashMap shims (read by key only). The calculate helpers return the maps the fields store (hub-headers.md §7.1).
 *
 * @author Sarynth, Neon
 */
class Influence : public runtime::Immortal {
private:
	// fieldmap: Java assigns a new LinkedHashMap (Influence.java:40), fieldmap guesses HashMap from the Map type; SM_INFLUENCE_RATIO writes the order
	runtime::Field<runtime::Ref<runtime::RcLinkedHashMap<int32_t, runtime::Ref<runtime::RcHashMap<SiegeRace, int32_t>>>>> influencesByWorld{};
	runtime::Field<runtime::Ref<runtime::RcHashMap<SiegeRace, int32_t>>> globalInfluences{};
	runtime::Field<float> elyosInfluenceRate{};
	runtime::Field<float> asmoInfluenceRate{};
	runtime::Field<float> balaurInfluenceRate{};
	Influence();
	~Influence();
public:
	static Influence& getInstance(); // Java singleton
	void recalculateInfluence();
private:
	runtime::Ref<runtime::RcLinkedHashMap<int32_t, runtime::Ref<runtime::RcHashMap<SiegeRace, int32_t>>>> calculateFortressWorldInfluences();
	runtime::Ref<runtime::RcHashMap<SiegeRace, int32_t>> calculateGlobalInfluences(
		runtime::RcLinkedHashMap<int32_t, runtime::Ref<runtime::RcHashMap<SiegeRace, int32_t>>>& influencesByWorld);
public:
	float getElyosInfluenceRate() const { return this->elyosInfluenceRate.get(); }
	float getAsmodianInfluenceRate() const { return this->asmoInfluenceRate.get(); }
	float getBalaurInfluenceRate() const { return this->balaurInfluenceRate.get(); }
	int32_t getInfluence(SiegeRace race);
	int32_t getInfluence(int32_t worldId, SiegeRace race);
	/** Java returns the live key set of influencesByWorld; C++: a snapshot in its order */
	std::vector<int32_t> getInfluenceRelevantWorldIds();
	/** @return int containing dmg modifier for disadvantaged race */
	int32_t getPvpRaceBonusRatio(Race attRace);
private:
	int32_t calculatePvpRaceBonusRatio(float ownInfluence, float enemyInfluence);
};

} // namespace aion::gameserver::model::siege
