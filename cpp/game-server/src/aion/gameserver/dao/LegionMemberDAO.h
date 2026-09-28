#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"

namespace aion::gameserver::dao {

/**
 * Class that is responsible for storing/loading legion data
 *
 * @author Simple
 */
class LegionMemberDAO {
public:
	static bool isIdUsed(int32_t playerObjId);
	static bool saveNewLegionMember(model::team::legion::LegionMember& legionMember);
	static void storeLegionMember(model::team::legion::LegionMember& legionMember);
	static runtime::Ref<model::team::legion::LegionMember> loadLegionMember(int32_t playerObjId);
	static std::vector<int32_t> loadLegionMembers(int32_t legionId);
	static void deleteLegionMember(int32_t playerObjId);
	static bool setRank(int32_t playerId, model::team::legion::LegionRank legionRank);
};

} // namespace aion::gameserver::dao
