#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/challenge/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/model/templates/challenge/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder. The
 * constructor is ported (it only creates the maps and logs).
 *
 * @author ViAl
 */
class ChallengeTaskService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcHashMap<int32_t, int32_t>>> taskAcceptTownIds{AION_LOCK_CLASS(ChallengeTaskService::taskAcceptTownIds#stripe)};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<model::challenge::ChallengeTask>>>> cityTasks{AION_LOCK_CLASS(ChallengeTaskService::cityTasks#stripe)};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<model::challenge::ChallengeTask>>>> legionTasks{AION_LOCK_CLASS(ChallengeTaskService::legionTasks#stripe)};
public:
	static ChallengeTaskService& getInstance(); // Java singleton
private:
	ChallengeTaskService();
	~ChallengeTaskService();
public:
	void showTaskList(model::gameobjects::player::Player& player, model::templates::challenge::ChallengeType challengeType, int32_t ownerId);
private:
	std::vector<runtime::Ptr<model::challenge::ChallengeTask>> buildTaskList(model::gameobjects::player::Player& player, model::templates::challenge::ChallengeType challengeType, int32_t ownerId, int32_t ownerLevel);
public:
	void onChallengeQuestFinish(model::gameobjects::player::Player& player, int32_t questId);
	void onAcceptTask(model::gameobjects::player::Player& player, int32_t questId);
private:
	void onCityTaskFinish(model::gameobjects::player::Player& player, const model::templates::challenge::ChallengeTaskTemplate* taskTemplate, int32_t questId);
	runtime::Ptr<model::challenge::ChallengeTask> getChallengeTask(model::gameobjects::player::Player& player, const model::templates::challenge::ChallengeTaskTemplate* taskTemplate, int32_t townId);
	void onLegionTaskFinish(model::gameobjects::player::Player& player, const model::templates::challenge::ChallengeTaskTemplate* taskTemplate, int32_t questId);
public:
	bool canRaiseLegionLevel(model::team::legion::Legion& legion, model::gameobjects::player::Player& actingPlayer);
};

} // namespace aion::gameserver::services
