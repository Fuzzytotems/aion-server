#include "aion/gameserver/services/AutoGroupService.h"

#include <optional>
#include <string>

#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AGQuestion.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/AutoInstance.h"
#include "aion/gameserver/model/autogroup/EntryRequestType.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/autogroup/AutoGroupUtility.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"
#include "aion/gameserver/services/instance/PvPArenaService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
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

using model::autogroup::AGQuestion;
using model::autogroup::AutoGroup;
using model::autogroup::AutoGroupType;
using model::autogroup::AutoInstance;
using model::autogroup::EntryRequestType;
using model::autogroup::LookingForParty;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_AUTO_GROUP;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::autogroup::AutoGroupUtility;
using utils::PacketSendUtility;

using LookingParties = runtime::RcArrayList<runtime::Ref<LookingForParty>>;

/** Java: agt.getTemplate(), dereferenced (a missing template is Java's NullPointerException) */
static const AutoGroup& templateOf(AutoGroupType agt) {
	const AutoGroup* found = model::autogroup::getTemplate(agt);
	if (found == nullptr)
		throw runtime::NullPointerException("AutoGroupType without auto group template");
	return *found;
}

AutoGroupService::AutoGroupService() = default;

AutoGroupService::~AutoGroupService() = default;

AutoGroupService& AutoGroupService::getInstance() {
	static AutoGroupService instance; // Java SingletonHolder
	return instance;
}

// Java AutoGroupService.java:51-79
void AutoGroupService::startLooking(Player& player, int32_t maskId, EntryRequestType ert) {
	std::optional<AutoGroupType> agt = model::autogroup::getAGTByMaskId(maskId);
	if (!agt || !canRegister(player, ert, *agt))
		return;
	runtime::Ref<LookingParties> lfps(lookingParties.computeIfAbsent(maskId, [](int32_t) {
		return LookingParties::create(AION_LOCK_CLASS(AutoGroupService::lookingParties));
	}));
	runtime::Ptr<LookingForParty> lfp;
	SYNCHRONIZED(*lfps) {
		lfp = getSearchEntry(player.getObjectId(), searchEntriesSnapshot(runtime::Ptr<LookingParties>(lfps)));
		if (lfp != nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ALREADY_REGISTERED(templateOf(*agt).getInstanceMapId()));
			return;
		}
		runtime::Ref<LookingForParty> created = LookingForParty::create(player, ert, maskId);
		lfp = runtime::Ptr<LookingForParty>(created);
		lfps->add(created);
		AutoGroupUtility::sendSuccessfulRegistration(*lfp, player.getName(), *agt, maskId);
		if (configs::main::AutoGroupConfig::ANNOUNCE_BATTLEGROUND_REGISTRATIONS.load() && model::autogroup::isPeriodicInstance(*agt) &&
			ert == EntryRequestType::GROUP_ENTRY) {
			int64_t sameRace = 0;
			for (const runtime::Ptr<LookingForParty>& s : lfps->snapshot())
				if (s->getRace() == player.getRace())
					sameRace++;
			if (sameRace == 1) {
				const model::Race race = player.getRace();
				const AutoGroupType type = *agt;
				PacketSendUtility::broadcastToWorld(
					network::aion::serverpackets::SM_MESSAGE(0, "",
						utils::ChatUtil::l10n(model::getL10nId(race)) + " have registered for " + utils::ChatUtil::l10n(model::autogroup::getL10nId(type)) + ".",
						model::ChatType::BRIGHT_YELLOW_CENTER),
					[race, type](Player& p) { return p.getRace() != race && model::autogroup::isInLvlRange(type, p.getLevel()); });
			}
		}
		if (!checkInstancesForOpenQuickEntries(*lfp, maskId))
			checkQueueForNewMatches(maskId);
	}
}

// Java AutoGroupService.java:81-117: the queue in its natural order (LookingForParty.compareTo); the first party a new instance takes and the
// following ones up to READY make the instance
void AutoGroupService::checkQueueForNewMatches(int32_t maskId) {
	runtime::Ptr<LookingParties> queuedParties = lookingParties.get(maskId);
	if (queuedParties == nullptr || queuedParties->isEmpty())
		return;
	std::optional<AutoGroupType> agt = model::autogroup::getAGTByMaskId(maskId);
	if (!agt)
		return;
	SYNCHRONIZED(*queuedParties) {
		queuedParties->sort(nullptr);
		for (int32_t i = 0; i < queuedParties->size(); i++) {
			runtime::Ref<AutoInstance> autoInstance = model::autogroup::createAutoInstance(*agt);
			runtime::Ptr<LookingForParty> lfp = queuedParties->get(i);
			AGQuestion question = autoInstance->addLookingForParty(*lfp);
			if (question == AGQuestion::FAILED)
				continue;
			std::vector<runtime::Ptr<LookingForParty>> filteredParties;
			filteredParties.push_back(lfp);
			if (question != AGQuestion::READY) {
				for (int32_t j = i + 1; j < queuedParties->size(); j++) {
					lfp = queuedParties->get(j);
					question = autoInstance->addLookingForParty(*lfp);
					if (question != AGQuestion::FAILED) {
						filteredParties.push_back(lfp);
						if (question == AGQuestion::READY)
							break;
					}
				}
			}
			if (question == AGQuestion::READY) {
				createNewInstance(*autoInstance, *agt, filteredParties, maskId);
				break;
			}
		}
	}
}

// Java AutoGroupService.java:119-133
void AutoGroupService::createNewInstance(AutoInstance& autoInstance, AutoGroupType agt, const std::vector<runtime::Ptr<LookingForParty>>& filteredParties,
	int32_t maskId) {
	runtime::Ptr<world::WorldMapInstance> mapInstance = instance::InstanceService::getNextAvailableInstance(templateOf(agt).getInstanceMapId(), 0,
		model::autogroup::getDifficultId(agt), nullptr, autoInstance.getMaxPlayers(), false);
	if (mapInstance == nullptr) // Java: autoInstance.onInstanceCreate(null) then instance.getMapId()
		throw runtime::NullPointerException("InstanceService.getNextAvailableInstance");
	autoInstance.onInstanceCreate(*mapInstance);
	autoInstances.put(runtime::Ref<world::WorldMapInstance>(mapInstance), runtime::Ref<AutoInstance>(autoInstance));
	for (const runtime::Ptr<LookingForParty>& lfp : filteredParties) {
		removeSearchEntry(*lfp);
		lfp->setStartEnterTime();
		for (int32_t id : lfp->getMembers().keySet()) {
			searchAndRemoveAdditionalRegistrations(id);
			AutoGroupUtility::sendWindowToPlayerIfOnline(id, maskId, 4);
		}
	}
}

// Java AutoGroupService.java:135-148
bool AutoGroupService::checkInstancesForOpenQuickEntries(LookingForParty& lfp, int32_t maskId) {
	if (lfp.getEntryRequestType() != EntryRequestType::QUICK_GROUP_ENTRY || lfp.isOnStartEnterTask())
		return false;
	for (const runtime::Ptr<AutoInstance>& autoInstance : autoInstances.values()) {
		if (templateOf(autoInstance->getAutoGroupType()).getMaskId() == maskId && autoInstance->addLookingForParty(lfp) == AGQuestion::ADDED) {
			removeSearchEntry(lfp);
			lfp.setStartEnterTime();
			AutoGroupUtility::sendWindowToPlayerIfOnline(lfp.getLeaderObjId(), maskId, 4);
			searchAndRemoveAdditionalRegistrations(lfp.getLeaderObjId());
			return true;
		}
	}
	return false;
}

// Java AutoGroupService.java:150-167
void AutoGroupService::checkQueueForQuickEntries(AutoInstance& autoInstance) {
	int32_t maskId = templateOf(autoInstance.getAutoGroupType()).getMaskId();
	runtime::Ptr<LookingParties> parties = lookingParties.get(maskId);
	if (parties == nullptr || parties->isEmpty())
		return;
	SYNCHRONIZED(*parties) {
		for (const runtime::Ptr<LookingForParty>& lfp : parties->snapshot()) {
			if (lfp->getEntryRequestType() == EntryRequestType::QUICK_GROUP_ENTRY && !lfp->isOnStartEnterTask() &&
				autoInstance.addLookingForParty(*lfp) == AGQuestion::ADDED) {
				removeSearchEntry(*lfp);
				lfp->setStartEnterTime();
				AutoGroupUtility::sendWindowToPlayerIfOnline(lfp->getLeaderObjId(), maskId, 4);
				searchAndRemoveAdditionalRegistrations(lfp->getLeaderObjId());
				return;
			}
		}
	}
}

// Java AutoGroupService.java:169-185
void AutoGroupService::searchAndRemoveAdditionalRegistrations(int32_t objectId) {
	std::vector<runtime::Ptr<LookingForParty>> partiesToRemove = getSearchEntries(objectId);
	for (const runtime::Ptr<LookingForParty>& lfp : partiesToRemove) {
		int32_t maskId = lfp->getMaskId();
		if (lfp->isLeader(objectId)) {
			removeSearchEntry(*lfp);
			penaliseParty(*lfp);
			for (int32_t id : lfp->getMembers().keySet())
				AutoGroupUtility::sendWindowToPlayerIfOnline(id, maskId, 2);
		} else {
			lfp->unregisterMember(objectId);
			AutoGroupUtility::sendWindowToPlayerIfOnline(objectId, maskId, 2);
			penalisePlayerAndScheduleRemoval(objectId);
			checkQueueForNewMatches(maskId);
		}
	}
}

// Java AutoGroupService.java:187-197
void AutoGroupService::pressEnter(Player& player, int32_t instanceMaskId) {
	runtime::Ptr<AutoInstance> autoInstance = getAutoInstance(player, instanceMaskId);
	if (autoInstance == nullptr)
		return;
	if (player.isInGroup())
		model::team::group::PlayerGroupService::removePlayer(player);
	if (player.isInAlliance())
		model::team::alliance::PlayerAllianceService::removePlayer(player);
	autoInstance->onPressEnter(player);
	PacketSendUtility::sendPacket(player, SM_AUTO_GROUP(instanceMaskId, 5));
}

void AutoGroupService::onEnterInstance(model::gameobjects::player::Player& player) {
	if (player.isInInstance()) {
		int32_t obj = player.getObjectId();
		runtime::Ptr<model::autogroup::AutoInstance> autoInstance = autoInstances.get(worldMapInstanceKey(player));
		if (autoInstance && autoInstance->getRegisteredAGPlayers().containsKey(obj))
			autoInstance->onEnterInstance(player);
	}
}

// Java AutoGroupService.java:208-218
void AutoGroupService::cancelEnter(Player& player, int32_t instanceMaskId) {
	runtime::Ptr<AutoInstance> autoInstance = getAutoInstance(player, instanceMaskId);
	if (autoInstance != nullptr) {
		int32_t objectId = player.getObjectId();
		autoInstance->unregister(player);
		penalisePlayerAndScheduleRemoval(objectId);
		destroyOrAddPlayersFromQuickEntries(*autoInstance);
		PacketSendUtility::sendPacket(player, SM_AUTO_GROUP(instanceMaskId, 2));
	}
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
			for (const runtime::Ptr<AutoInstance>& autoInstance : autoInstances.values()) {
				cancelEnter(player, templateOf(autoInstance->getAutoGroupType()).getMaskId());
			}
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

// Java AutoGroupService.java:271-276. A list of the mask is there: the party was registered in it (Java: a null list throws)
void AutoGroupService::removeSearchEntry(LookingForParty& lfp) {
	runtime::Ptr<LookingParties> lfps = lookingParties.get(lfp.getMaskId());
	if (lfps == nullptr)
		throw runtime::NullPointerException("lookingParties.get(" + std::to_string(lfp.getMaskId()) + ")");
	SYNCHRONIZED(*lfps) {
		lfps->remove(runtime::Ptr<LookingForParty>(lfp));
	}
}

// Java AutoGroupService.java:278-285
void AutoGroupService::onLeaveInstance(Player& player) {
	runtime::Ptr<AutoInstance> autoInstance = autoInstances.get(worldMapInstanceKey(player));
	if (autoInstance != nullptr && autoInstance->getRegisteredAGPlayers().containsKey(player.getObjectId())) {
		autoInstance->onLeaveInstance(player);
		destroyOrAddPlayersFromQuickEntries(*autoInstance);
	}
	instance::PeriodicInstanceManager::getInstance().checkAndSendOpenRegistrations(player);
}

// Java AutoGroupService.java:287-305
bool AutoGroupService::canRegister(Player& player, EntryRequestType ert, AutoGroupType agt) {
	int32_t mapId = templateOf(agt).getInstanceMapId();
	int32_t instanceMaskId = templateOf(agt).getMaskId();
	if (!model::autogroup::isInLvlRange(agt, player.getLevel())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL());
		return false;
	} else if ((model::autogroup::isPvPFFAArena(agt) || model::autogroup::isPvPSoloArena(agt) || model::autogroup::isHarmonyArena(agt) ||
				   model::autogroup::isGloryArena(agt)) &&
		!instance::PvPArenaService::isPvPArenaAvailable(player, agt)) {
		return false;
	} else if (AutoGroupUtility::hasCoolDown(player, mapId)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_MAKE_INSTANCE_COOL_TIME());
		return false;
	}
	switch (ert) {
		case EntryRequestType::NEW_GROUP_ENTRY:
			return AutoGroupUtility::canRegisterNewEntry(player, agt);
		case EntryRequestType::QUICK_GROUP_ENTRY:
			return AutoGroupUtility::canRegisterQuickEntry(player, agt);
		case EntryRequestType::GROUP_ENTRY:
			return AutoGroupUtility::canRegisterGroupEntry(player, agt, mapId, instanceMaskId);
	}
	return false; // unreachable: the switch covers every EntryRequestType
}

// Java AutoGroupService.java:307-309
void AutoGroupService::penaliseParty(LookingForParty& lfp) {
	for (int32_t id : lfp.getMembers().keySet())
		penalisePlayerAndScheduleRemoval(id);
}

// Java AutoGroupService.java:311-318. The task (fieldmap AutoGroupService@L293:45) captures the immortal singleton and the object id.
void AutoGroupService::penalisePlayerAndScheduleRemoval(int32_t objectId) {
	if (penalties.add(objectId)) {
		utils::ThreadPoolManager::getInstance().schedule({this}, [this, objectId] {
			penalties.remove(objectId);
			instance::PeriodicInstanceManager::getInstance().checkAndSendOpenRegistrations(objectId);
		}, 10000);
	}
}

// Java AutoGroupService.java:320-324
void AutoGroupService::stopRegistrationsByMaskId(int32_t maskId) {
	runtime::Ptr<LookingParties> parties = lookingParties.remove(maskId);
	if (parties != nullptr && !parties->isEmpty())
		for (const runtime::Ptr<LookingForParty>& lfp : parties->snapshot())
			for (int32_t id : lfp->getMembers().keySet())
				AutoGroupUtility::sendWindowToPlayerIfOnline(id, maskId, 2);
}

// Java AutoGroupService.java:326-328
void AutoGroupService::cancelRegistration(Player& player, int32_t maskId) {
	cancelRegistration(getSearchEntry(player, maskId), player, maskId);
}

// Java AutoGroupService.java:330-345
void AutoGroupService::cancelRegistration(runtime::Ptr<LookingForParty> lfp, Player& player, int32_t maskId) {
	int32_t objectId = player.getObjectId();
	if (lfp != nullptr) {
		if (lfp->isLeader(objectId)) {
			runtime::Ptr<LookingParties> parties = lookingParties.get(maskId);
			if (parties == nullptr) // Java: lookingParties.get(maskId).remove(lfp) on a removed list
				throw runtime::NullPointerException("lookingParties.get(" + std::to_string(maskId) + ")");
			parties->remove(lfp);
			penaliseParty(*lfp);
			for (int32_t id : lfp->getMembers().keySet())
				AutoGroupUtility::sendWindowToPlayerIfOnline(id, maskId, 2);
		} else {
			lfp->unregisterMember(objectId);
			AutoGroupUtility::sendWindowToPlayer(player, maskId, 2);
			penalisePlayerAndScheduleRemoval(objectId);
			checkQueueForNewMatches(maskId);
		}
	}
}

// Java AutoGroupService.java:347-350
void AutoGroupService::destroyOrAddPlayersFromQuickEntries(AutoInstance& autoInstance) {
	if (!destroyIfPossible(autoInstance) && templateOf(autoInstance.getAutoGroupType()).canRegisterQuickEntry())
		checkQueueForQuickEntries(autoInstance);
}

// Java AutoGroupService.java:352-360
bool AutoGroupService::destroyIfPossible(AutoInstance& autoInstance) {
	runtime::Ptr<world::WorldMapInstance> mapInstance = autoInstance.getInstance();
	if (autoInstance.getRegisteredAGPlayers().isEmpty()) {
		if (mapInstance == nullptr) // Java: instance.getPlayersInside() of an instance not created yet
			throw runtime::NullPointerException("AutoInstance.getInstance()");
		bool anyOnline = false;
		for (const runtime::Ptr<Player>& p : mapInstance->getPlayersInside())
			anyOnline = anyOnline || p->isOnline();
		if (!anyOnline) { // Java: stream().noneMatch(Player::isOnline)
			autoInstances.remove(runtime::Ref<world::WorldMapInstance>(mapInstance));
			instance::InstanceService::destroyInstance(*mapInstance);
			return true;
		}
	}
	return false;
}

// Java AutoGroupService.java:362-368
runtime::Ptr<AutoInstance> AutoGroupService::getAutoInstance(Player& player, int32_t instanceMaskId) {
	for (const runtime::Ptr<AutoInstance>& autoInstance : autoInstances.values())
		if (templateOf(autoInstance->getAutoGroupType()).getMaskId() == instanceMaskId &&
			autoInstance->getRegisteredAGPlayers().containsKey(player.getObjectId()))
			return autoInstance;
	return nullptr;
}

bool AutoGroupService::isInAutoInstance(model::gameobjects::player::Player& player) {
	return autoInstances.containsKey(worldMapInstanceKey(player));
}

} // namespace aion::gameserver::services
