#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/autogroup/AutoInstanceHandler.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::autogroup {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz, Estrayl
 */
class AutoInstance : public runtime::RefCounted, public AutoInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<AGPlayer>> registeredAGPlayers{AION_LOCK_CLASS(AutoInstance::registeredAGPlayers#stripe)};
	const AutoGroupType agt;
	runtime::Field<runtime::Ref<world::WorldMapInstance>> instance{};
	runtime::Field<int64_t> startInstanceTime{};

	explicit AutoInstance(AutoGroupType agt);

	bool removeItem(gameobjects::player::Player& player, int32_t itemId, int64_t requiredCount);

public:
	void onInstanceCreate(world::WorldMapInstance& instance) override;

	AGQuestion addLookingForParty(LookingForParty& lookingForParty) override;

	void onEnterInstance(gameobjects::player::Player& player) override;

	void onLeaveInstance(gameobjects::player::Player& player) override;

	void onPressEnter(gameobjects::player::Player& player) override;

	void unregister(gameobjects::player::Player& player) override;

protected:
	bool isRegistrationDisabled(LookingForParty& lfp);

public:
	AutoGroupType getAutoGroupType() const { return this->agt; }

	runtime::ConcurrentHashMap<int32_t, runtime::Ref<AGPlayer>>& getRegisteredAGPlayers() { return this->registeredAGPlayers; }

	runtime::Ptr<world::WorldMapInstance> getInstance() const { return this->instance.get(); }

	int64_t getStartInstanceTime() const { return this->startInstanceTime.get(); }

protected:
	std::vector<runtime::Ptr<AGPlayer>> getAGPlayersByRace(Race race);

	std::vector<runtime::Ptr<AGPlayer>> getAGPlayersByClass(PlayerClass playerClass);

	std::vector<runtime::Ptr<gameobjects::player::Player>> getPlayersByRace(Race race);

public:
	virtual int32_t getMaxPlayers();

protected:
	~AutoInstance() override;
};

} // namespace aion::gameserver::model::autogroup
