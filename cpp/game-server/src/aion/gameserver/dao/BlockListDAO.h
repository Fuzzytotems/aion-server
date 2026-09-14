#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * Responsible for saving and loading data on players' block lists
 *
 * @author Ben
 */
class BlockListDAO {
public:
	static bool addBlockedUser(int32_t playerObjId, int32_t objIdToBlock, std::string_view reason);
	static bool delBlockedUser(int32_t playerObjId, int32_t objIdToDelete);
	static runtime::Ref<model::gameobjects::player::BlockList> load(int32_t playerObjId);
	static bool setReason(int32_t playerObjId, int32_t blockedPlayerObjId, std::string_view reason);
};

} // namespace aion::gameserver::dao
