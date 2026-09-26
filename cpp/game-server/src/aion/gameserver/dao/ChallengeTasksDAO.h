#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/challenge/fwd.h"
#include "aion/gameserver/model/templates/challenge/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ViAl
 */
class ChallengeTasksDAO {
public:
	static std::unordered_map<int32_t, runtime::Ref<model::challenge::ChallengeTask>> load(int32_t ownerId,
		model::templates::challenge::ChallengeType type);
	static void storeTask(model::challenge::ChallengeTask& task);
private:
	static void insertQuestEntry(model::challenge::ChallengeTask& task, model::challenge::ChallengeQuest& quest);
	static void updateQuestEntry(model::challenge::ChallengeTask& task, model::challenge::ChallengeQuest& quest);
};

} // namespace aion::gameserver::dao
