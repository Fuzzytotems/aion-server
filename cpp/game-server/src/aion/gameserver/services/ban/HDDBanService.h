#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/services/ban/fwd.h"

namespace aion::gameserver::services::ban {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author ViAl
 */
class HDDBanService : public runtime::Immortal {
private:
	runtime::HashMap<std::string, commons::database::Timestamp> bannedSerials{AION_LOCK_CLASS(HDDBanService::bannedSerials)}; // Java: = new HashMap<>()
	HDDBanService();
	~HDDBanService();
public:
	static HDDBanService& getInstance(); // Java singleton
	void addBan(std::string_view serial, std::optional<commons::database::Timestamp> banTime);
	void removeBan(std::string_view serial);
	void loadBan(std::string_view serial, int64_t banTime);
	bool isBanned(std::string_view serial);
};

} // namespace aion::gameserver::services::ban
