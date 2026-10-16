#pragma once

#include <any>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Ben, Neon
 */
class ServerVariablesDAO {
public:
	static std::optional<int32_t> loadInt(std::string_view var);
	static std::optional<int64_t> loadLong(std::string_view var);
	static bool store(std::string_view var, const std::any& value);
	bool delete_(std::string_view var);
private:
	/** @return absent (Java null) if the variable is not stored */
	static std::optional<std::string> load(std::string_view var);
};

} // namespace aion::gameserver::dao
