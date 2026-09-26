#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/Connection.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author MrPoke, vlog, Rolandas
 */
class PlayerQuestListDAO {
public:
	static runtime::Ref<model::gameobjects::player::QuestStateList> load(int32_t playerObjId);
	static void store(model::gameobjects::player::Player& player);
private:
	static void addQuests(commons::database::Connection& con, int32_t playerId,
		const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states);
	static void updateQuests(commons::database::Connection& con, int32_t playerId,
		const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states);
	static void deleteQuest(commons::database::Connection& con, int32_t playerId,
		const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states, const std::unordered_set<int32_t>& questIds);
};

} // namespace aion::gameserver::dao
