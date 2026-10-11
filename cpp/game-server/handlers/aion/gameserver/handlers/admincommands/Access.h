#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"
#include "aion/gameserver/runtime/collections/HashMap.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //access: grants and removes chat command accesses, and lowers the GM's own access level for tests.
 *
 * @author ViAl, Neon
 */
class Access : public AdminCommand {
public:
	Access();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	runtime::HashMap<int32_t, int8_t> oldAccessLevels{AION_LOCK_CLASS(Access::oldAccessLevels)};
};

} // namespace aion::gameserver::handlers::admincommands
