#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/findGroup/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/services/findgroup/fwd.h"

namespace aion::gameserver::services::findgroup {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * TemporaryPlayerTeam<?> is the erased TemporaryPlayerTeam (hub-headers.md §8.1).
 *
 * @author cura, MrPoke
 */
class FindGroupService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::findGroup::GroupRecruitment>> recruitments{AION_LOCK_CLASS(FindGroupService::recruitments#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::findGroup::GroupApplication>> applications{AION_LOCK_CLASS(FindGroupService::applications#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::findGroup::ServerWideGroup>> instanceGroups{AION_LOCK_CLASS(FindGroupService::instanceGroups#stripe)}; // Java: = new ConcurrentHashMap<>()
	FindGroupService();
	~FindGroupService();
public:
	void showRecruitments(model::gameobjects::player::Player& player);
	void showApplications(model::gameobjects::player::Player& player);
	runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment> removeRecruitment(model::team::TemporaryPlayerTeam& team);
	runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment> removeRecruitment(model::gameobjects::player::Player& player, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3);
private:
	runtime::Ptr<model::gameobjects::findGroup::GroupRecruitment> removeRecruitment(int32_t playerOrTeamId, int8_t serverId, int8_t unk1, int8_t unk2, int8_t unk3);
public:
	void removeApplication(model::gameobjects::player::Player& player);
	void addRecruitment(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType);
	void addApplication(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType, int32_t classId, int32_t level);
	void updateRecruitment(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType);
	void updateApplication(model::gameobjects::player::Player& player, std::string_view message, int32_t groupType, int32_t classId, int32_t level);
	void showInstanceGroups(model::gameobjects::player::Player& player, bool isUpdate);
	void showInstanceGroups(model::gameobjects::player::Player& player, model::gameobjects::Npc& portalNpc);
	void registerInstanceGroup(model::gameobjects::player::Player& player, int32_t instanceMaskId, std::string_view message, int32_t minMembers);
	void updateInstanceGroup(model::gameobjects::player::Player& player, std::string_view message);
	void removeInstanceGroup(model::gameobjects::player::Player& player);
	void showInstanceGroupMembersInfo(model::gameobjects::player::Player& player, int32_t playerObjectId);
	void sendInstanceApplication(model::gameobjects::player::Player& applicant, int32_t playerOrTeamId);
	void sendInstanceApplicationResult(model::gameobjects::player::Player& responder, int32_t applicantId, int8_t instanceApplicationReply);
	void onJoinedTeam(model::gameobjects::player::Player& player);
	void onLogout(model::gameobjects::player::Player& player);
	static FindGroupService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services::findgroup
