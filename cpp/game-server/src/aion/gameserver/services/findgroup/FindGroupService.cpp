#include "aion/gameserver/services/findgroup/FindGroupService.h"

#include <vector>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/findGroup/FindGroupEntry.h"
#include "aion/gameserver/model/gameobjects/findGroup/GroupApplication.h"
#include "aion/gameserver/model/gameobjects/findGroup/GroupRecruitment.h"
#include "aion/gameserver/model/gameobjects/findGroup/ServerWideGroup.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIND_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::findgroup {

using model::gameobjects::findGroup::FindGroupEntry;
using model::gameobjects::findGroup::GroupApplication;
using model::gameobjects::findGroup::GroupRecruitment;
using model::gameobjects::findGroup::ServerWideGroup;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_FIND_GROUP;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

namespace {

/** Java: (byte) NetworkConfig.GAMESERVER_ID */
int8_t gameServerId() {
	return static_cast<int8_t>(configs::network::NetworkConfig::GAMESERVER_ID.load());
}

} // namespace

FindGroupService::FindGroupService() = default;

FindGroupService::~FindGroupService() = default;

FindGroupService& FindGroupService::getInstance() {
	static FindGroupService instance; // Java SingletonHolder
	return instance;
}

void FindGroupService::showRecruitments(Player& player) {
	std::vector<runtime::Ptr<FindGroupEntry>> recruitmentList;
	for (const runtime::Ptr<GroupRecruitment>& r : this->recruitments.values()) {
		if (r->getRace() == player.getRace())
			recruitmentList.push_back(runtime::Ptr<FindGroupEntry>(*r));
	}
	PacketSendUtility::sendPacket(player, SM_FIND_GROUP(0, recruitmentList));
}

void FindGroupService::showApplications(Player& player) {
	std::vector<runtime::Ptr<FindGroupEntry>> applicationList;
	for (const runtime::Ptr<GroupApplication>& r : this->applications.values()) {
		if (r->getPlayer()->getRace() == player.getRace())
			applicationList.push_back(runtime::Ptr<FindGroupEntry>(*r));
	}
	PacketSendUtility::sendPacket(player, SM_FIND_GROUP(4, applicationList));
}

runtime::Ptr<GroupRecruitment> FindGroupService::removeRecruitment(model::team::TemporaryPlayerTeam& team) {
	return removeRecruitment(team.getTeamId(), gameServerId(), 0, 0, 0);
}

runtime::Ptr<GroupRecruitment> FindGroupService::removeRecruitment(Player& player, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3) {
	int32_t teamId = player.getCurrentTeamId();
	return removeRecruitment(teamId == 0 ? player.getObjectId() : teamId, serverId, unk1, unk2, unk3);
}

runtime::Ptr<GroupRecruitment> FindGroupService::removeRecruitment(int32_t playerOrTeamId, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3) {
	runtime::Ptr<GroupRecruitment> recruitment = recruitments.remove(playerOrTeamId);
	if (recruitment) {
		const model::Race race = recruitment->getRace();
		PacketSendUtility::broadcastToWorld(SM_FIND_GROUP(playerOrTeamId, serverId, unk1, unk2, unk3),
			[race](Player& p) { return p.getRace() == race; });
	}
	return recruitment;
}

void FindGroupService::removeApplication(Player& player) {
	runtime::Ptr<GroupApplication> application = applications.remove(player.getObjectId());
	if (application) {
		const model::Race race = application->getPlayer()->getRace();
		PacketSendUtility::broadcastToWorld(SM_FIND_GROUP(player.getObjectId()), [race](Player& p) { return p.getRace() == race; });
	}
}

void FindGroupService::addRecruitment(Player& player, std::string_view message, int32_t groupType) {
	runtime::Ptr<model::gameobjects::AionObject> playerOrTeam = player.getCurrentTeam();
	if (!playerOrTeam)
		playerOrTeam = runtime::Ptr<model::gameobjects::AionObject>(player);
	runtime::Ref<GroupRecruitment> recruitment = GroupRecruitment::create(*playerOrTeam, message, groupType);
	recruitments.put(playerOrTeam->getObjectId(), recruitment);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PARTY_MATCH_OFFER_PARTY_POSTED());
	showRecruitments(player); // necessary if player switched tabs before adding this entry (client bug)
}

void FindGroupService::addApplication(Player& player, std::string_view message, int32_t groupType, int32_t classId, int32_t level) {
	runtime::Ref<GroupApplication> application = GroupApplication::create(player, message, groupType, classId, level);
	applications.put(player.getObjectId(), application);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PARTY_MATCH_SEEK_PARTY_POSTED());
	showApplications(player); // necessary if player switched tabs before adding this entry (client bug)
}

void FindGroupService::updateRecruitment(Player& player, std::string_view message, int32_t groupType) {
	int32_t teamId = player.getCurrentTeamId();
	runtime::Ptr<GroupRecruitment> recruitment = recruitments.get(teamId == 0 ? player.getObjectId() : teamId);
	if (recruitment) {
		recruitment->setMessage(message);
		recruitment->setGroupType(groupType);
		recruitment->updateLastUpdate();
	}
}

void FindGroupService::updateApplication(Player& player, std::string_view message, int32_t groupType, int32_t classId, int32_t level) {
	runtime::Ptr<GroupApplication> application = applications.get(player.getObjectId());
	if (application) {
		application->setMessage(message);
		application->setGroupType(groupType);
		application->setClassId(classId);
		application->setLevel(level);
		application->updateLastUpdate();
	}
}

void FindGroupService::showInstanceGroups(Player& player, bool isUpdate) {
	std::vector<runtime::Ptr<FindGroupEntry>> instanceGroupList;
	for (const runtime::Ptr<ServerWideGroup>& group : this->instanceGroups.values()) {
		if (group->getRace() == player.getRace())
			instanceGroupList.push_back(runtime::Ptr<FindGroupEntry>(*group));
	}

	if (!isUpdate && configs::main::GroupConfig::FORM_INSTANCE_GROUP_ANYWHERE.load()) {
		const std::vector<int32_t>* instanceMaskIds = nullptr;
		if (runtime::Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(player.getTarget()))
			instanceMaskIds = dataholders::DataManager::AUTO_GROUP->getRecruitableInstanceMaskIds(npc->getNpcId());
		if (instanceMaskIds == nullptr)
			PacketSendUtility::sendPacket(player, SM_FIND_GROUP(dataholders::DataManager::AUTO_GROUP->getRecruitableInstanceMaskIds()));
		else
			PacketSendUtility::sendPacket(player, SM_FIND_GROUP(*instanceMaskIds));
	}

	PacketSendUtility::sendPacket(player, SM_FIND_GROUP(10, instanceGroupList));
}

void FindGroupService::showInstanceGroups(Player& player, model::gameobjects::Npc& portalNpc) {
	const std::vector<int32_t>* instanceMaskIds = dataholders::DataManager::AUTO_GROUP->getRecruitableInstanceMaskIds(portalNpc.getNpcId());
	if (instanceMaskIds != nullptr)
		PacketSendUtility::sendPacket(player, SM_FIND_GROUP(*instanceMaskIds));
}

void FindGroupService::registerInstanceGroup(Player& player, int32_t instanceMaskId, std::string_view message, int32_t minMembers) {
	runtime::Ref<ServerWideGroup> instanceGroup = ServerWideGroup::create(player, instanceMaskId, minMembers, message);
	instanceGroups.put(player.getObjectId(), instanceGroup);
	PacketSendUtility::sendPacket(player, SM_FIND_GROUP(14, std::vector<runtime::Ptr<FindGroupEntry>>{runtime::Ptr<FindGroupEntry>(*instanceGroup)}));
}

void FindGroupService::updateInstanceGroup(Player& player, std::string_view message) {
	runtime::Ptr<ServerWideGroup> instanceGroup = instanceGroups.get(player.getObjectId());
	if (instanceGroup) {
		instanceGroup->setMessage(message);
		instanceGroup->setLastUpdate();
		showInstanceGroups(player, true);
	}
}

void FindGroupService::removeInstanceGroup(Player& player) {
	instanceGroups.remove(player.getObjectId());
	showInstanceGroups(player, true);
}

void FindGroupService::showInstanceGroupMembersInfo(Player& player, int32_t playerObjectId) {
	runtime::Ptr<ServerWideGroup> instanceGroup = instanceGroups.get(playerObjectId);
	if (instanceGroup)
		PacketSendUtility::sendPacket(player, SM_FIND_GROUP(16, std::vector<runtime::Ptr<FindGroupEntry>>{runtime::Ptr<FindGroupEntry>(*instanceGroup)}));
}

void FindGroupService::sendInstanceApplication(Player& applicant, int32_t playerOrTeamId) {
	runtime::Ptr<Player> player = world::World::getInstance().getPlayer(playerOrTeamId);
	if (player)
		PacketSendUtility::sendPacket(*player, SM_FIND_GROUP(applicant));
}

void FindGroupService::sendInstanceApplicationResult(Player& responder, int32_t applicantId, int8_t instanceApplicationReply) {
	runtime::Ptr<Player> applicant = world::World::getInstance().getPlayer(applicantId);
	if (applicant) {
		if (instanceApplicationReply == 1) {
			runtime::Ptr<ServerWideGroup> instanceGroup = instanceGroups.get(responder.getObjectId());
			if (instanceGroup) {
				// custom: invite to team to keep it simple, as cross-server recruitment is currently not implemented.
				// for more info about official server implementation, see CM_/SM_FIND_GROUP action codes 18-25 and
				// https://forum.aion.gameforge.com/forum/thread/742-server-wide-recruitment-guide-by-kelekelio/
				if (instanceGroup->getMinMembers() <= 6)
					model::team::group::PlayerGroupService::inviteToGroup(responder, *applicant);
				else
					model::team::alliance::PlayerAllianceService::inviteToAlliance(responder, *applicant);
			}
		} else {
			PacketSendUtility::sendPacket(*applicant,
				network::aion::serverpackets::SM_MESSAGE(responder, utils::ChatUtil::l10n(1400217), model::ChatType::WHISPER));
		}
	}
}

void FindGroupService::onJoinedTeam(Player& player) {
	runtime::Ptr<ServerWideGroup> instanceGroup = instanceGroups.get(player.getObjectId());
	// custom: team is used as a proxy for a server-wide instance group (forming a team removes instance group registrations on official servers)
	if (instanceGroup && static_cast<int32_t>(instanceGroup->getMembers().size()) >= instanceGroup->getMinMembers())
		instanceGroups.remove(player.getObjectId());
	removeApplication(player);
	runtime::Ptr<GroupRecruitment> recruitment = removeRecruitment(player.getObjectId(), gameServerId(), 0, 0, 16);
	runtime::Ptr<model::team::TemporaryPlayerTeam> team = player.getCurrentTeam();
	if (recruitment && team->isLeader(player))
		addRecruitment(player, recruitment->getMessage(), recruitment->getGroupType());
	else if (team->isFull())
		removeRecruitment(team->getObjectId(), gameServerId(), 0, 0, 0);
}

void FindGroupService::onLogout(Player& player) {
	recruitments.remove(player.getObjectId());
	applications.remove(player.getObjectId());
	instanceGroups.remove(player.getObjectId());
}

} // namespace aion::gameserver::services::findgroup
