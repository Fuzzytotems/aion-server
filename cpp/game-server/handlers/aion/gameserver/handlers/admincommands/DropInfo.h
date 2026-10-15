#pragma once

#include <array>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //dropinfo: lists the drops of the targeted npc (custom, global and event drop groups).
 *
 * @author Oliver, AionCool, Bobobear, Neon
 */
class DropInfo : public AdminCommand {
public:
	DropInfo();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	/** Java: a List&lt;GlobalRule&gt; (the global drop data's rules, or the active events' rules) */
	std::string collectDropInfo(const std::string& dropGroupPrefix, const std::vector<const GlobalRule*>& rules, Npc& npc,
		DropModifiers& dropModifiers, bool isAllowedDefaultGlobalDropNpc, bool showAll, std::array<int32_t, 2>& counts);
};

} // namespace aion::gameserver::handlers::admincommands
