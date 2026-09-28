#include "aion/gameserver/custom/instance/CustomInstanceService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::custom::instance {

CustomInstanceService::CustomInstanceService() {
	// Java's static initializer runs before the first constructor: the leaderboard window id is allocated here (class comment)
	static_cast<void>(leaderboardWindowObjectId());
}

CustomInstanceService::~CustomInstanceService() = default;

int32_t CustomInstanceService::leaderboardWindowObjectId() {
	static const int32_t id = utils::idfactory::IDFactory::getInstance().nextId();
	return id;
}

bool CustomInstanceService::canEnter(int32_t playerId) {
	AION_UNPORTED();
}

void CustomInstanceService::onEnter(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

CustomInstanceRank CustomInstanceService::loadOrCreateRank(int32_t playerId) {
	AION_UNPORTED();
}

bool CustomInstanceService::resetEntryCooldown(int32_t playerId) {
	AION_UNPORTED();
}

bool CustomInstanceService::updateLastEntry(int32_t playerId, int64_t newEntryTime) {
	AION_UNPORTED();
}

bool CustomInstanceService::changePlayerRank(int32_t playerId, int32_t newRank, int32_t achievedDps) {
	AION_UNPORTED();
}

bool CustomInstanceService::storeNewRankData(CustomInstanceRank& rankObj) {
	AION_UNPORTED();
}

void CustomInstanceService::changeRank(CustomInstanceRank& rankObj, int32_t newRank) {
	AION_UNPORTED();
}

void CustomInstanceService::recordPlayerModelEntry(model::gameobjects::player::Player& player, skillengine::model::Skill& skill,
	runtime::Ptr<model::gameobjects::VisibleObject> target) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<neuralnetwork::PlayerModelEntry>> CustomInstanceService::loadPlayerModelEntries(int32_t playerId) {
	AION_UNPORTED();
}

void CustomInstanceService::saveNewPlayerModelEntries(int32_t playerId) {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcArrayList<runtime::Ref<neuralnetwork::PlayerModelEntry>>> CustomInstanceService::getPlayerModelEntries(int32_t playerId) {
	AION_UNPORTED();
}

void CustomInstanceService::openLeaderboard(model::gameobjects::player::Player& player, model::Race race) {
	AION_UNPORTED();
}

CustomInstanceService& CustomInstanceService::getInstance() {
	static CustomInstanceService instance; // Java: SingletonHolder
	return instance;
}

} // namespace aion::gameserver::custom::instance
