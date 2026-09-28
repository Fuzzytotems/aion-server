#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author cura
 */
class PlayerPasskeyDAO {
public:
	static void insertPlayerPasskey(int32_t accountId, std::string_view passkey);
	static bool updatePlayerPasskey(int32_t accountId, std::string_view oldPasskey, std::string_view newPasskey);
	static bool updateForcePlayerPasskey(int32_t accountId, std::string_view newPasskey);
	static bool checkPlayerPasskey(int32_t accountId, std::string_view passkey);
	static bool existCheckPlayerPasskey(int32_t accountId);
};

} // namespace aion::gameserver::dao
