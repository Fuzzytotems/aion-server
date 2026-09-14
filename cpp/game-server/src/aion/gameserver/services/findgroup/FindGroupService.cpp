#include "aion/gameserver/services/findgroup/FindGroupService.h"

#include "aion/gameserver/model/gameobjects/findGroup/GroupApplication.h"
#include "aion/gameserver/model/gameobjects/findGroup/GroupRecruitment.h"
#include "aion/gameserver/model/gameobjects/findGroup/ServerWideGroup.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::findgroup {

FindGroupService::FindGroupService() = default;

FindGroupService::~FindGroupService() = default;

FindGroupService& FindGroupService::getInstance() {
	static FindGroupService instance; // Java SingletonHolder
	return instance;
}

void FindGroupService::showRecruitments(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void FindGroupService::showApplications(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment> FindGroupService::removeRecruitment(model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment> FindGroupService::removeRecruitment(model::gameobjects::player::Player& player, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment> FindGroupService::removeRecruitment(int32_t playerOrTeamId, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3) {
	AION_UNPORTED();
}

void FindGroupService::removeApplication(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void FindGroupService::addRecruitment(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType) {
	AION_UNPORTED();
}

void FindGroupService::addApplication(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType, int32_t classId, int32_t level) {
	AION_UNPORTED();
}

void FindGroupService::updateRecruitment(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType) {
	AION_UNPORTED();
}

void FindGroupService::updateApplication(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType, int32_t classId, int32_t level) {
	AION_UNPORTED();
}

void FindGroupService::showInstanceGroups(model::gameobjects::player::Player& player, bool isUpdate) {
	AION_UNPORTED();
}

void FindGroupService::showInstanceGroups(model::gameobjects::player::Player& player, model::gameobjects::Npc& portalNpc) {
	AION_UNPORTED();
}

void FindGroupService::registerInstanceGroup(model::gameobjects::player::Player& player, int32_t instanceMaskId, std::string_view message, int32_t minMembers) {
	AION_UNPORTED();
}

void FindGroupService::updateInstanceGroup(model::gameobjects::player::Player& player, std::string_view message) {
	AION_UNPORTED();
}

void FindGroupService::removeInstanceGroup(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void FindGroupService::showInstanceGroupMembersInfo(model::gameobjects::player::Player& player, int32_t playerObjectId) {
	AION_UNPORTED();
}

void FindGroupService::sendInstanceApplication(model::gameobjects::player::Player& applicant, int32_t playerOrTeamId) {
	AION_UNPORTED();
}

void FindGroupService::sendInstanceApplicationResult(model::gameobjects::player::Player& responder, int32_t applicantId, int8_t instanceApplicationReply) {
	AION_UNPORTED();
}

void FindGroupService::onJoinedTeam(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void FindGroupService::onLogout(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::findgroup
