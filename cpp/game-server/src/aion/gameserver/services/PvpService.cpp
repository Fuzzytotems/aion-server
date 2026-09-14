#include "aion/gameserver/services/PvpService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/templates/bounty/KillBountyTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("KILL_LOG");

PvpService::PvpService() {
	AION_UNPORTED();
}

PvpService& PvpService::getInstance() {
	static PvpService instance; // Java SingletonHolder
	return instance;
}

void PvpService::sendBountyReward(model::gameobjects::player::Player& player, model::templates::bounty::BountyType type, int32_t killScore) {
	AION_UNPORTED();
}

void PvpService::finalizeHeadhuntingSeason() {
	AION_UNPORTED();
}

void PvpService::doReward(model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

runtime::Ptr<model::event::Headhunter> PvpService::getHeadhunterById(int32_t objId) {
	AION_UNPORTED();
}

void PvpService::doReward(model::gameobjects::player::Player& victim, float apWinMulti) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> PvpService::findMembersToCountKillFor(model::gameobjects::player::Player& winner,
	model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

void PvpService::logKill(model::gameobjects::player::Player& winner, model::gameobjects::player::Player& victim,
	const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& assistedGroup) {
	AION_UNPORTED();
}

bool PvpService::rewardPlayerTeam(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& teamMember,
	model::gameobjects::player::Player& victim, int32_t damage, int32_t totalDamage, float apWinMulti) {
	AION_UNPORTED();
}

void PvpService::updateKillQuests(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& killers,
	model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

runtime::Ptr<model::event::Headhunter> PvpService::getHeadhunter(int32_t hunterId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
