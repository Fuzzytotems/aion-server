#include "aion/gameserver/services/AutoGroupService.h"

#include "aion/gameserver/model/autogroup/AutoInstance.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::services {

/** Java autoInstances.get(player.getWorldMapInstance()): ConcurrentHashMap.get(null) throws NullPointerException */
static runtime::Ref<world::WorldMapInstance> worldMapInstanceKey(model::gameobjects::player::Player& player) {
	runtime::Ptr<world::WorldMapInstance> mapInstance = player.getWorldMapInstance();
	if (!mapInstance)
		throw runtime::NullPointerException("player.getWorldMapInstance()");
	return runtime::Ref<world::WorldMapInstance>(mapInstance);
}

/**
 * C++: Java passes the live list, which getSearchEntry iterates inside synchronized (parties). The frozen signature takes a vector, so callers
 * pass searchEntriesSnapshot(list), taken under the list's Monitor (the same lock); Java null is the empty vector.
 */
static std::vector<runtime::Ptr<model::autogroup::LookingForParty>> searchEntriesSnapshot(
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<model::autogroup::LookingForParty>>> parties) {
	std::vector<runtime::Ptr<model::autogroup::LookingForParty>> entries;
	if (parties) {
		for (const runtime::Ptr<model::autogroup::LookingForParty>& lfp : parties->snapshot())
			entries.push_back(lfp);
	}
	return entries;
}

AutoGroupService::AutoGroupService() = default;

AutoGroupService::~AutoGroupService() = default;

AutoGroupService& AutoGroupService::getInstance() {
	static AutoGroupService instance; // Java SingletonHolder
	return instance;
}

void AutoGroupService::startLooking(model::gameobjects::player::Player& player, int32_t maskId, model::autogroup::EntryRequestType ert) {
	AION_UNPORTED();
}

void AutoGroupService::checkQueueForNewMatches(int32_t maskId) {
	AION_UNPORTED();
}

void AutoGroupService::createNewInstance(model::autogroup::AutoInstance& autoInstance, model::autogroup::AutoGroupType agt, const std::vector<runtime::Ptr<model::autogroup::LookingForParty>>& filteredParties, int32_t maskId) {
	AION_UNPORTED();
}

bool AutoGroupService::checkInstancesForOpenQuickEntries(model::autogroup::LookingForParty& lfp, int32_t maskId) {
	AION_UNPORTED();
}

void AutoGroupService::checkQueueForQuickEntries(model::autogroup::AutoInstance& autoInstance) {
	AION_UNPORTED();
}

void AutoGroupService::searchAndRemoveAdditionalRegistrations(int32_t objectId) {
	AION_UNPORTED();
}

void AutoGroupService::pressEnter(model::gameobjects::player::Player& player, int32_t instanceMaskId) {
	AION_UNPORTED();
}

void AutoGroupService::onEnterInstance(model::gameobjects::player::Player& player) {
	if (player.isInInstance()) {
		int32_t obj = player.getObjectId();
		runtime::Ptr<model::autogroup::AutoInstance> autoInstance = autoInstances.get(worldMapInstanceKey(player));
		if (autoInstance && autoInstance->getRegisteredAGPlayers().containsKey(obj))
			autoInstance->onEnterInstance(player);
	}
}

void AutoGroupService::cancelEnter(model::gameobjects::player::Player& player, int32_t instanceMaskId) {
	AION_UNPORTED();
}

void AutoGroupService::onPlayerLogin(model::gameobjects::player::Player& player) {
	instance::PeriodicInstanceManager::getInstance().checkAndSendOpenRegistrations(player);
}

bool AutoGroupService::isSearching(model::gameobjects::player::Player& player, int32_t maskId) {
	return static_cast<bool>(getSearchEntry(player.getObjectId(), searchEntriesSnapshot(lookingParties.get(maskId))));
}

// lint: L7 Java's synchronized (parties) of the int overload is the list Monitor that searchEntriesSnapshot takes (the vector signature is frozen)
runtime::Ptr<model::autogroup::LookingForParty> AutoGroupService::getSearchEntry(model::gameobjects::player::Player& player, int32_t maskId) {
	return getSearchEntry(player.getObjectId(), searchEntriesSnapshot(lookingParties.get(maskId)));
}

// lint: L7 Java's synchronized (parties) is the list Monitor that searchEntriesSnapshot takes before this call (the vector signature is frozen)
runtime::Ptr<model::autogroup::LookingForParty> AutoGroupService::getSearchEntry(int32_t playerObjectId, const std::vector<runtime::Ptr<model::autogroup::LookingForParty>>& parties) {
	for (const runtime::Ptr<model::autogroup::LookingForParty>& lfp : parties)
		if (lfp->isMember(playerObjectId))
			return lfp;
	return nullptr;
}

std::vector<runtime::Ptr<model::autogroup::LookingForParty>> AutoGroupService::getSearchEntries(int32_t playerObjectId) {
	std::vector<runtime::Ptr<model::autogroup::LookingForParty>> entries;
	for (const auto& parties : lookingParties.values().toVector()) {
		if (runtime::Ptr<model::autogroup::LookingForParty> entry = getSearchEntry(playerObjectId, searchEntriesSnapshot(parties)))
			entries.push_back(entry);
	}
	return entries;
}

void AutoGroupService::onLogout(model::gameobjects::player::Player& player) {
	int32_t objectId = player.getObjectId();
	for (const runtime::Ptr<model::autogroup::LookingForParty>& lfp : getSearchEntries(objectId)) {
		if (lfp->isOnStartEnterTask()) {
			// Java: for each autoInstance: cancelEnter(player, autoInstance.getAutoGroupType().getTemplate().getMaskId()); the AutoGroupType
			// companion is not ported yet
			if (!autoInstances.isEmpty())
				AION_UNPORTED();
		} else if (lfp->isLeader(objectId)) {
			int32_t newLeader = 0;
			for (int32_t id : lfp->getMembers().keySet()) {
				if (id != objectId) {
					newLeader = id; // Java: findFirst
					break;
				}
			}
			lfp->setLeaderObjId(newLeader);
			if (lfp->getLeaderObjId() == 0) {
				removeSearchEntry(*lfp);
			}
		} else {
			lfp->unregisterMember(objectId);
			checkQueueForNewMatches(lfp->getMaskId());
		}
	}

	runtime::Ptr<model::autogroup::AutoInstance> autoInstance = autoInstances.get(worldMapInstanceKey(player));
	if (autoInstance && autoInstance->getRegisteredAGPlayers().containsKey(objectId)) {
		destroyIfPossible(*autoInstance);
	}
}

void AutoGroupService::removeSearchEntry(model::autogroup::LookingForParty& lfp) {
	AION_UNPORTED();
}

void AutoGroupService::onLeaveInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AutoGroupService::canRegister(model::gameobjects::player::Player& player, model::autogroup::EntryRequestType ert, model::autogroup::AutoGroupType agt) {
	AION_UNPORTED();
}

void AutoGroupService::penaliseParty(model::autogroup::LookingForParty& lfp) {
	AION_UNPORTED();
}

// callback at AutoGroupService.java:293 (fieldmap key AutoGroupService@L293:45)
void AutoGroupService::penalisePlayerAndScheduleRemoval(int32_t objectId) {
	AION_UNPORTED();
}

void AutoGroupService::stopRegistrationsByMaskId(int32_t maskId) {
	AION_UNPORTED();
}

void AutoGroupService::cancelRegistration(model::gameobjects::player::Player& player, int32_t maskId) {
	AION_UNPORTED();
}

void AutoGroupService::cancelRegistration(runtime::Ptr<model::autogroup::LookingForParty> lfp, model::gameobjects::player::Player& player, int32_t maskId) {
	AION_UNPORTED();
}

void AutoGroupService::destroyOrAddPlayersFromQuickEntries(model::autogroup::AutoInstance& autoInstance) {
	AION_UNPORTED();
}

bool AutoGroupService::destroyIfPossible(model::autogroup::AutoInstance& autoInstance) {
	AION_UNPORTED();
}

runtime::Ptr<model::autogroup::AutoInstance> AutoGroupService::getAutoInstance(model::gameobjects::player::Player& player, int32_t instanceMaskId) {
	AION_UNPORTED();
}

bool AutoGroupService::isInAutoInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
