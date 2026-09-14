#include "aion/gameserver/services/ChallengeTaskService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/challenge/ChallengeTask.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.ChallengeTaskService");

ChallengeTaskService::ChallengeTaskService() {
	log.info("ChallengeTaskService initialized.");
}

ChallengeTaskService::~ChallengeTaskService() = default;

ChallengeTaskService& ChallengeTaskService::getInstance() {
	static ChallengeTaskService instance; // Java SingletonHolder
	return instance;
}

void ChallengeTaskService::showTaskList(model::gameobjects::player::Player& player, model::templates::challenge::ChallengeType challengeType, int32_t ownerId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::challenge::ChallengeTask>> ChallengeTaskService::buildTaskList(model::gameobjects::player::Player& player, model::templates::challenge::ChallengeType challengeType, int32_t ownerId, int32_t ownerLevel) {
	AION_UNPORTED();
}

void ChallengeTaskService::onChallengeQuestFinish(model::gameobjects::player::Player& player, int32_t questId) {
	AION_UNPORTED();
}

void ChallengeTaskService::onAcceptTask(model::gameobjects::player::Player& player, int32_t questId) {
	AION_UNPORTED();
}

void ChallengeTaskService::onCityTaskFinish(model::gameobjects::player::Player& player, const model::templates::challenge::ChallengeTaskTemplate* taskTemplate, int32_t questId) {
	AION_UNPORTED();
}

runtime::Ptr<model::challenge::ChallengeTask> ChallengeTaskService::getChallengeTask(model::gameobjects::player::Player& player, const model::templates::challenge::ChallengeTaskTemplate* taskTemplate, int32_t townId) {
	AION_UNPORTED();
}

void ChallengeTaskService::onLegionTaskFinish(model::gameobjects::player::Player& player, const model::templates::challenge::ChallengeTaskTemplate* taskTemplate, int32_t questId) {
	AION_UNPORTED();
}

bool ChallengeTaskService::canRaiseLegionLevel(model::team::legion::Legion& legion, model::gameobjects::player::Player& actingPlayer) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
