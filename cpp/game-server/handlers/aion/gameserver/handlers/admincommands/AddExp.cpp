#include "aion/gameserver/handlers/admincommands/AddExp.h"

#include <algorithm>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(AddExp);

AddExp::AddExp()
	: AdminCommand("addexp", "Increases/decreases a players experience points.", "<exp> - The experience points to add (may be negative).\n") {
}

// Java AddExp.java:20-32
void AddExp::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(admin.getTarget());
	Player& target = targetPlayer != nullptr ? *targetPlayer : admin;
	int64_t exp = commons::utils::parseLong(params[0]);
	// Java long addition wraps on overflow
	int64_t resultExp = std::max<int64_t>(0, static_cast<int64_t>(static_cast<uint64_t>(target.getCommonData()->getExp()) + static_cast<uint64_t>(exp)));
	target.getCommonData()->setExp(resultExp);
	sendInfo(admin, "You added " + std::to_string(exp) + " exp points to " + name(target) + ".");
}

} // namespace aion::gameserver::handlers::admincommands
