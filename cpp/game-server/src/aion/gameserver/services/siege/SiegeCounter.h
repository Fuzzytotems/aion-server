#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services::siege {

class SiegeCounter : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new EnumMap<>(SiegeRace.class)
	runtime::EnumMap<model::siege::SiegeRace, runtime::Ref<SiegeRaceCounter>> siegeRaceCounters{AION_LOCK_CLASS(SiegeCounter::siegeRaceCounters)};

protected:
	SiegeCounter();

public:
	static runtime::Ref<SiegeCounter> create();

	void addDamage(model::gameobjects::Creature& creature, int32_t damage);

	void addAbyssPoints(model::gameobjects::player::Player& player, int32_t ap);

	runtime::Ptr<SiegeRaceCounter> getRaceCounter(model::siege::SiegeRace race);

	void addRaceDamage(model::siege::SiegeRace race, int32_t damage);

	runtime::Ptr<SiegeRaceCounter> getWinnerRaceCounter(model::siege::SiegeRace fallbackRace);

protected:
	~SiegeCounter() override;
};

} // namespace aion::gameserver::services::siege
