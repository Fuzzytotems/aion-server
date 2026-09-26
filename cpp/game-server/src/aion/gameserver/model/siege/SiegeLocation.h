#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::model::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Sarynth, Source, Wakizashi
 */
class SiegeLocation : public runtime::RefCounted, public world::zone::handler::ZoneHandler {
	AION_MAKE_REF_FRIEND
public:
	static constexpr int32_t STATE_INVULNERABLE = 0;
	static constexpr int32_t STATE_VULNERABLE = 1;

private:
	const templates::siegelocation::SiegeLocationTemplate* template_;
	runtime::ArrayList<runtime::Ref<world::zone::SiegeZoneInstance>> zones{AION_LOCK_CLASS(SiegeLocation::zones)}; // Java: = new ArrayList<>()
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<gameobjects::Creature>> creatures{AION_LOCK_CLASS(SiegeLocation::creatures#stripe)};
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<gameobjects::player::Player>> players{AION_LOCK_CLASS(SiegeLocation::players#stripe)};
	runtime::Field<SiegeRace> siegeRace{SiegeRace::BALAUR};
	runtime::Field<int32_t> legionId{};
	runtime::Field<bool> vulnerable{};
	runtime::Field<int32_t> nextState{};
	runtime::Field<bool> isUnderShield_{};
	runtime::Field<bool> canTeleport{true};
	runtime::Field<int32_t> occupiedCount{};
	runtime::Field<int32_t> factionBalance{};

protected:
	explicit SiegeLocation(const templates::siegelocation::SiegeLocationTemplate* template_);

public:
	static runtime::Ref<SiegeLocation> create(const templates::siegelocation::SiegeLocationTemplate* value);

	const templates::siegelocation::SiegeLocationTemplate* getTemplate() const { return this->template_; }

	int32_t getLocationId();

	int32_t getWorldId();

	SiegeType getType();

	int32_t getSiegeDuration();

	std::vector<const templates::siegelocation::SiegeReward*> getRewards();

	virtual SiegeRace getRace() { return this->siegeRace.get(); }

	void setRace(SiegeRace value) { this->siegeRace.set(value); }

	int32_t getLegionId() const { return this->legionId.get(); }

	void setLegionId(int32_t value) { this->legionId.set(value); }

	/** Next State: 0 invulnerable 1 vulnerable */
	virtual int32_t getNextState() { return this->nextState.get(); }

	void setNextState(int32_t value) { this->nextState.set(value); }

	bool isVulnerable() const { return this->vulnerable.get(); }

	bool isUnderShield() const { return this->isUnderShield_.get(); }

	int32_t getOccupiedCount() const { return this->occupiedCount.get(); }

	void increaseOccupiedCount();

	void setOccupiedCount(int32_t value) { this->occupiedCount.set(value); }

	/**
	 * Gets the balance between factions of this location. A positive value between 1 and 9 means asmodians are handicapped and will therefore get a
	 * support buff. Vice versa, -1 to -9 means elyos are handicapped and will get a support buff.<br>
	 * 0 = balanced, no support buff<br>
	 * ±1 = weakest support buff<br>
	 * ±9 = strongest support buff<br>
	 * <br>
	 * Should only be used for {@link FortressLocation}.
	 */
	int32_t getFactionBalance() const { return this->factionBalance.get(); }

	/**
	 * In- or decrements the faction balance whether adjustment is positive or negative and limits
	 * it to 9 or -9;
	 */
	void adjustFactionBalance(int32_t adjustment);

	void setFactionBalance(int32_t value) { this->factionBalance.set(value); }

	void setUnderShield(bool value) { this->isUnderShield_.set(value); }

	bool isCanTeleport(runtime::Ptr<gameobjects::player::Player> player);

	int32_t getLegionGp();

	void setCanTeleport(bool value) { this->canTeleport.set(value); }

	void setVulnerable(bool value) { this->vulnerable.set(value); }

	int32_t getInfluenceValue();

	runtime::ArrayList<runtime::Ref<world::zone::SiegeZoneInstance>>& getZone() { return this->zones; }

	void addZone(world::zone::SiegeZoneInstance& zone);

	bool isInsideLocation(gameobjects::Creature& creature);

	bool isInsideLocation(float x, float y, float z);

	virtual void clearLocation();

	void onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) override;

	void onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) override;

	void forEachCreature(const std::function<void(gameobjects::Creature&)>& consumer);

	void forEachPlayer(const std::function<void(gameobjects::player::Player&)>& consumer);

	/** C++ only: ZoneHandler retain the object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	~SiegeLocation() override;
};

} // namespace aion::gameserver::model::siege
