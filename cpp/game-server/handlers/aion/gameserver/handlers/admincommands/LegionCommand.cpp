#include "aion/gameserver/handlers/admincommands/LegionCommand.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(LegionCommand);

LegionCommand::LegionCommand()
	: AdminCommand("legion", "Modifies a legion.", R"(info <legion name> - List legion members.
add <legion name> <player name> - Adds the player to the legion.
kick <player name> - Kicks the player from their legion.
disband <legion name> - Disbands the legion.
rename <legion name> <new name> - Changes the legion's name.
setbg <player name> - Appoints the player as brigade general of their legion.
setlevel <legion name> <level> - Changes the legion's level.
setpoints <legion name> <points> - Changes the legion's contributing points.
)") {
}

// Java LegionCommand.java:37-132
void LegionCommand::execute(Player& player, std::span<const std::string> params) {
	namespace StringUtils = commons::utils::StringUtils;
	using model::team::legion::Legion;
	using model::team::legion::LegionMember;
	services::LegionService& legionService = services::LegionService::getInstance();
	if (params.size() < 2) {
		sendInfo(player);
		return;
	}

	if (StringUtils::equalsIgnoreCase(params[0], "disband")) {
		runtime::Ptr<Legion> legion = getLegion(params[1]);
		legionService.disbandLegion(*legion);
		sendInfo(player, "Legion " + legion->getName() + " was disbanded.");
	} else if (StringUtils::equalsIgnoreCase(params[0], "setlevel") && params.size() >= 3) {
		runtime::Ptr<Legion> legion = getLegion(params[1]);
		const int32_t level = commons::utils::parseInt(params[2]);
		if (level < 1 || level > 8) {
			sendInfo(player, "Legion level must be between 1 and 8.");
			return;
		} else if (level == legion->getLegionLevel()) {
			sendInfo(player, "Legion " + params[1] + " is already on level " + std::to_string(level));
			return;
		}
		const int32_t old = legion->getLegionLevel();
		legionService.changeLevel(*legion, level, true);
		sendInfo(player, "Legion " + legion->getName() + " level was changed from " + std::to_string(old) + " to " + std::to_string(level));
	} else if (StringUtils::equalsIgnoreCase(params[0], "setpoints") && params.size() >= 3) {
		runtime::Ptr<Legion> legion = getLegion(params[1]);
		const int64_t points = commons::utils::parseLong(params[2]);
		if (points < 1) {
			sendInfo(player, "Points must be larger than zero.");
			return;
		}
		const int64_t old = legion->getContributionPoints();
		legionService.setContributionPoints(*legion, points, true);
		sendInfo(player, "Legion " + legion->getName() + " contribution points were changed from " + std::to_string(old) + " to " + std::to_string(points));
	} else if (StringUtils::equalsIgnoreCase(params[0], "rename") && params.size() >= 3) {
		runtime::Ptr<Legion> legion = getLegion(params[1]);
		const std::string old = legion->getName();
		if (legionService.tryRename(*legion, params[2], player, std::nullopt))
			sendInfo(player, "Legion " + old + " was renamed to " + legion->getName() + ".");
	} else if (StringUtils::equalsIgnoreCase(params[0], "info")) {
		runtime::Ptr<Legion> legion = getLegion(params[1]);
		sendInfo(player, "Legion name: " + legion->getName());
		sendInfo(player, "Level: " + std::to_string(legion->getLegionLevel()));
		sendInfo(player, "Contribution points: " + std::to_string(legion->getContributionPoints()));
		sendInfo(player, "Members (" + std::to_string(legion->getMemberIds()->size()) + "):");
		for (const runtime::Ptr<LegionMember>& lm : legion->getMembers()) {
			const std::string brigadeGeneralInfo = lm->isBrigadeGeneral() ? ", brigade general" : "";
			sendInfo(player, "\t" + lm->getName() + " (lv " + std::to_string(lm->getLevel()) + " " + std::string(xml::enumName(lm->getPlayerClass())) +
								 brigadeGeneralInfo + ")");
		}
	} else if (StringUtils::equalsIgnoreCase(params[0], "kick")) {
		runtime::Ptr<Player> target = world::World::getInstance().getPlayer(utils::Util::convertName(params[1]));
		if (!target)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(params[1]));
		// correction (owner's decision 2026-10-13): Java reads target.getLegionMember().isBrigadeGeneral() for a target in no legion - a
		// NullPointerException answered "<Error while executing command>"; the port tells the GM instead (docs/deviations/C1.md)
		else if (!target->isLegionMember())
			sendInfo(player, name(*target) + " is not a member of any legion.");
		else if (target->getLegionMember()->isBrigadeGeneral())
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_CAN_BANISH_MASTER());
		else if (legionService.leaveLegion(*target, true))
			sendInfo(player, name(*target) + " was kicked from the legion.");
		else
			sendInfo(player, name(*target) + " could not be kicked from the legion.");
	} else if (StringUtils::equalsIgnoreCase(params[0], "add") && params.size() >= 3) {
		runtime::Ptr<Legion> legion = getLegion(params[1]);
		runtime::Ptr<Player> target = world::World::getInstance().getPlayer(utils::Util::convertName(params[2]));
		if (!target)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(params[2]));
		else if (target->isLegionMember())
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_HE_IS_OTHER_GUILD_MEMBER(target->getName()));
		else if (legionService.addToLegion(*legion, *target, player))
			sendInfo(player, name(*target) + " was added to " + legion->getName());
	} else if (StringUtils::equalsIgnoreCase(params[0], "setbg")) {
		runtime::Ptr<model::gameobjects::player::PlayerCommonData> playerCommonData =
			PlayerService::getOrLoadPlayerCommonData(utils::Util::convertName(params[1]));
		if (!playerCommonData) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NO_USER_NAMED(params[1]));
			return;
		}
		runtime::Ptr<LegionMember> legionMember = legionService.getLegionMember(*playerCommonData);
		if (!legionMember) {
			sendInfo(player, playerCommonData->getName() + " is not a member of any legion.");
			return;
		}
		const std::string legionName = legionMember->getLegion()->getName();
		const std::string previousBrigadeGeneral = legionMember->getLegion()->getBrigadeGeneral()->getName();
		legionService.appointBrigadeGeneral(*legionMember);
		const std::string newBrigadeGeneral = legionMember->getLegion()->getBrigadeGeneral()->getName();
		sendInfo(player, "The brigade general of legion " + legionName + " was changed from " + previousBrigadeGeneral + " to " + newBrigadeGeneral + ".");
	} else {
		sendInfo(player);
	}
}

// Java LegionCommand.java:134-141
runtime::Ptr<model::team::legion::Legion> LegionCommand::getLegion(std::string_view nameValue) {
	std::string legionName(nameValue);
	std::replace(legionName.begin(), legionName.end(), '_', ' ');
	runtime::Ptr<model::team::legion::Legion> legion = services::LegionService::getInstance().getLegion(commons::utils::StringUtils::toLowerCase(legionName));
	if (!legion)
		throw runtime::IllegalArgumentException("Legion " + legionName + " does not exist.");
	return legion;
}

} // namespace aion::gameserver::handlers::admincommands
