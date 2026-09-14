#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/siege/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/services/mail/fwd.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic Siege<SL>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author SoulKeeper, Source
 */
class Siege : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean finished{AION_LOCK_CLASS(Siege::finished)}; // Java: = new AtomicBoolean()
	const runtime::Ref<SiegeCounter> siegeCounter;
	const runtime::Ref<model::siege::SiegeLocation> siegeLocation;
	runtime::Field<bool> bossKilled{};
	runtime::Field<runtime::Ref<model::gameobjects::siege::SiegeNpc>> boss{};
	runtime::Field<int64_t> startTime{};
	runtime::Field<bool> started{};

protected:
	explicit Siege(model::siege::SiegeLocation& siegeLocation);

public:
	void startSiege();

	void startSiege(int32_t locationId);

	void stopSiege();

	runtime::Ptr<model::siege::SiegeLocation> getSiegeLocation() const { return this->siegeLocation; }

	int32_t getSiegeLocationId();

	bool isBossKilled() const { return this->bossKilled.get(); }

	void setBossKilled(bool value) { this->bossKilled.set(value); }

	runtime::Ptr<model::gameobjects::siege::SiegeNpc> getBoss() const { return this->boss.get(); }

	void setBoss(runtime::Ptr<model::gameobjects::siege::SiegeNpc> boss);

	runtime::Ptr<SiegeCounter> getSiegeCounter() const { return this->siegeCounter; }

	runtime::Ptr<SiegeRaceCounter> getWinnerRaceCounter();

protected:
	virtual void onSiegeStart() = 0;

	virtual void onSiegeFinish() = 0;

public:
	virtual bool isEndless() = 0;

	virtual void onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) = 0;

	bool isStarted() const { return this->started.get(); }

	bool isFinished();

	int64_t getStartTime() const { return this->startTime.get(); }

protected:
	void initSiegeBoss();

	void spawnNpcs(int32_t locationId, model::siege::SiegeRace race, model::siege::SiegeModType type);

	void despawnNpcs(int32_t locationId);

	void broadcastState(model::siege::SiegeLocation& location);

	void broadcastUpdate(model::siege::SiegeLocation& location);

	void updateOutpostStatusByFortress(model::siege::FortressLocation& location);

	void sendRewardsToParticipants(SiegeRaceCounter& raceCounter, mail::SiegeResult raceResult);

public:
	std::string toString();

protected:
	~Siege() override;
};

} // namespace aion::gameserver::services::siege
