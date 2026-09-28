#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/fwd.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::network {

/**
 * C++: an Immortal singleton (fieldmap base Immortal; Java's eagerly created static instance). The logger is the .cpp logger.
 *
 * @author KID
 */
class BannedMacManager : public runtime::Immortal {
private:
	runtime::HashMap<std::string, runtime::Ref<BannedMacEntry>> bannedList{AION_LOCK_CLASS(BannedMacManager::bannedList)};

	BannedMacManager();
	~BannedMacManager();

public:
	static BannedMacManager& getInstance();

	/** Java final */
	void banAddress(std::string_view address, int64_t newTime, std::string_view details);

	/** Java final */
	bool unbanAddress(std::string_view address, std::string_view details);

	/** Java final */
	bool isBanned(std::string_view address);

	/** Java final */
	void dbLoad(std::string_view address, int64_t time, std::string_view details);

	void onEnd();
};

} // namespace aion::gameserver::network
