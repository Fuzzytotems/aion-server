#include "aion/gameserver/handlers/admincommands/Set.h"

#include <algorithm>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/services/ClassChangeService.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/utils/EnumValueOf.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Set);

Set::Set()
	: AdminCommand("set", "Changes various player attributes.",
		  "class <value> - Sets the class of the selected player.\n"
		  "level <value> - Sets the level of the selected player.\n"
		  "exp <value> - Sets the experience points of the selected player.\n"
		  "ap <value> - Sets the abyss points of the selected player.\n"
		  "gp <value> - Sets the glory points of the selected player.\n"
		  "Note: Any actions default to your character, if no player is targeted.\n") {
}

// Java Set.java:28-63
void Set::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(admin.getTarget());
	Player& target = targetPlayer != nullptr ? *targetPlayer : admin;

	if (params[0] == "class") {
		PlayerClass playerClass = utils::enumValueOf<PlayerClass>(commons::utils::StringUtils::toUpperCase(params[1])); // parity= PlayerClass playerClass = PlayerClass.valueOf(params[1].toUpperCase());
		ClassChangeService::setClass(target, playerClass, true, true);
	} else if (params[0] == "level") {
		int32_t level = std::min(GSConfig::PLAYER_MAX_LEVEL.load(), commons::utils::parseInt(params[1])); // parity= int level = Math.min(GSConfig.PLAYER_MAX_LEVEL, Integer.parseInt(params[1]));
		target.getCommonData()->setLevel(level);
		sendInfo(admin, "Set " + name(target) + "'s level to " + std::to_string(target.getLevel()));
	} else if (params[0] == "exp") {
		int64_t exp = commons::utils::parseLong(params[1]);
		target.getCommonData()->setExp(exp);
		sendInfo(admin, "Set exp of target to " + std::to_string(target.getCommonData()->getExp()));
	} else if (params[0] == "ap") {
		int32_t ap = commons::utils::parseInt(params[1]);
		AbyssPointsService::addAp(target, ap - target.getAbyssRank()->getAp());
		if (&target != &admin) {
			sendInfo(admin, "Set " + name(target) + "'s abyss points to " + std::to_string(target.getAbyssRank()->getAp()) + ".");
			sendInfo(target, "Admin set your abyss points to " + std::to_string(target.getAbyssRank()->getAp()) + ".");
		}
	} else if (params[0] == "gp") {
		int32_t gp = commons::utils::parseInt(params[1]);
		GloryPointsService::addGp(target.getObjectId(), gp - target.getAbyssRank()->getCurrentGP());
		if (&target != &admin) {
			sendInfo(admin, "Set " + name(target) + "'s glory points to " + std::to_string(target.getAbyssRank()->getCurrentGP()) + ".");
			sendInfo(target, "Admin set your glory points to " + std::to_string(target.getAbyssRank()->getCurrentGP()) + ".");
		}
	} else {
		sendInfo(admin);
	}
}

} // namespace aion::gameserver::handlers::admincommands
