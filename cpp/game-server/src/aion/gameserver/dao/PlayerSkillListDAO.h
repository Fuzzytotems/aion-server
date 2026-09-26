#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/Connection.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"

namespace aion::gameserver::dao {

/**
 * Created on: 15.07.2009 19:33:07 Edited On: 13.09.2009 19:48:00
 *
 * @author SoulKeeper, IceReaper, orfeo087, Avol, AEJTester
 */
class PlayerSkillListDAO {
public:
	static runtime::Ref<model::skill::PlayerSkillList> loadSkillList(int32_t playerId);
	/** Stores all player skills according to their persistence state */
	static bool storeSkills(model::gameobjects::player::Player& player);
private:
	static void store(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills);
	static void addSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
		const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills);
	static void updateSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
		const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills);
	static void deleteSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
		const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills);
};

} // namespace aion::gameserver::dao
