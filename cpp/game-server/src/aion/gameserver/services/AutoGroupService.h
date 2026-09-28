#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author xTz, Estrayl
 */
class AutoGroupService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<runtime::Ref<world::WorldMapInstance>, runtime::Ref<model::autogroup::AutoInstance>> autoInstances{AION_LOCK_CLASS(AutoGroupService::autoInstances#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::autogroup::LookingForParty>>>> lookingParties{AION_LOCK_CLASS(AutoGroupService::lookingParties#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentKeySet<int32_t> penalties{AION_LOCK_CLASS(AutoGroupService::penalties#stripe)}; // Java: = ConcurrentHashMap.newKeySet()
	AutoGroupService();
	~AutoGroupService();
public:
	void startLooking(model::gameobjects::player::Player& player, int32_t maskId, model::autogroup::EntryRequestType ert);
private:
	void checkQueueForNewMatches(int32_t maskId);
	void createNewInstance(model::autogroup::AutoInstance& autoInstance, model::autogroup::AutoGroupType agt, const std::vector<runtime::Ptr<model::autogroup::LookingForParty>>& filteredParties, int32_t maskId);
	bool checkInstancesForOpenQuickEntries(model::autogroup::LookingForParty& lfp, int32_t maskId);
	void checkQueueForQuickEntries(model::autogroup::AutoInstance& autoInstance);
	void searchAndRemoveAdditionalRegistrations(int32_t objectId);
public:
	void pressEnter(model::gameobjects::player::Player& player, int32_t instanceMaskId);
	void onEnterInstance(model::gameobjects::player::Player& player);
	void cancelEnter(model::gameobjects::player::Player& player, int32_t instanceMaskId);
	void onPlayerLogin(model::gameobjects::player::Player& player);
	bool isSearching(model::gameobjects::player::Player& player, int32_t maskId);
private:
	runtime::Ptr<model::autogroup::LookingForParty> getSearchEntry(model::gameobjects::player::Player& player, int32_t maskId);
	runtime::Ptr<model::autogroup::LookingForParty> getSearchEntry(int32_t playerObjectId, const std::vector<runtime::Ptr<model::autogroup::LookingForParty>>& parties);
	std::vector<runtime::Ptr<model::autogroup::LookingForParty>> getSearchEntries(int32_t playerObjectId);
public:
	void onLogout(model::gameobjects::player::Player& player);
private:
	void removeSearchEntry(model::autogroup::LookingForParty& lfp);
public:
	void onLeaveInstance(model::gameobjects::player::Player& player);
private:
	bool canRegister(model::gameobjects::player::Player& player, model::autogroup::EntryRequestType ert, model::autogroup::AutoGroupType agt);
	void penaliseParty(model::autogroup::LookingForParty& lfp);
	void penalisePlayerAndScheduleRemoval(int32_t objectId);
public:
	void stopRegistrationsByMaskId(int32_t maskId);
	void cancelRegistration(model::gameobjects::player::Player& player, int32_t maskId);
	void cancelRegistration(runtime::Ptr<model::autogroup::LookingForParty> lfp, model::gameobjects::player::Player& player, int32_t maskId);
private:
	void destroyOrAddPlayersFromQuickEntries(model::autogroup::AutoInstance& autoInstance);
public:
	bool destroyIfPossible(model::autogroup::AutoInstance& autoInstance);
private:
	runtime::Ptr<model::autogroup::AutoInstance> getAutoInstance(model::gameobjects::player::Player& player, int32_t instanceMaskId);
public:
	bool isInAutoInstance(model::gameobjects::player::Player& player);
	static AutoGroupService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
