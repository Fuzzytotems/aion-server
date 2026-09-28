#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ViAl
 */
class CommandsAccessDAO {
public:
	static std::unordered_map<int32_t, std::unordered_set<std::string>> loadAccesses();
	static void addAccess(int32_t playerId, std::string_view commandName);
	static void removeAccess(int32_t playerId, std::string_view commandName);
	static void removeAllAccesses(int32_t playerId);
};

} // namespace aion::gameserver::dao
