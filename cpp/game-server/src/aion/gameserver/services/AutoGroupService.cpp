#include "aion/gameserver/services/AutoGroupService.h"

#include "aion/gameserver/model/autogroup/AutoInstance.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::services {

AutoGroupService::AutoGroupService() = default;

AutoGroupService::~AutoGroupService() = default;

AutoGroupService& AutoGroupService::getInstance() {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

void AutoGroupService::cancelEnter(model::gameobjects::player::Player& player, int32_t instanceMaskId) {
	AION_UNPORTED();
}

void AutoGroupService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AutoGroupService::isSearching(model::gameobjects::player::Player& player, int32_t maskId) {
	AION_UNPORTED();
}

runtime::Ptr<model::autogroup::LookingForParty> AutoGroupService::getSearchEntry(model::gameobjects::player::Player& player, int32_t maskId) {
	AION_UNPORTED();
}

runtime::Ptr<model::autogroup::LookingForParty> AutoGroupService::getSearchEntry(int32_t playerObjectId, const std::vector<runtime::Ptr<model::autogroup::LookingForParty>>& parties) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::autogroup::LookingForParty>> AutoGroupService::getSearchEntries(int32_t playerObjectId) {
	AION_UNPORTED();
}

void AutoGroupService::onLogout(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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
