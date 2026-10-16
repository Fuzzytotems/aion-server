#include "aion/gameserver/handlers/admincommands/SiegeCommand.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/siege/BalaurAssaultService.h"
#include "aion/gameserver/utils/EnumValueOf.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(SiegeCommand);

SiegeCommand::SiegeCommand()
	: AdminCommand("siege", "Controls sieges and artifacts.", R"(locations - Shows info about all locations.
start <location ID> - Starts the siege at the given location.
stop <location ID> - Stops the siege at the given location.
capture <location ID> [elyos|asmodians|balaur|legionName|legionId] - Captures the fortress at given location.
assault <location ID> [delaySec] - Starts an assault at the given location.
)") {
}

// Java SiegeCommand.java:33-43
void SiegeCommand::execute(Player& player, std::span<const std::string> params) {
	const std::string command = params.empty() ? "" : commons::utils::StringUtils::toLowerCase(params[0]);
	if (command == "locations")
		listLocations(player);
	else if (command == "start")
		startSiege(player, *parseLocation(params));
	else if (command == "stop")
		stopSiege(player, *parseLocation(params));
	else if (command == "capture")
		capture(player, *parseLocation(params), params);
	else if (command == "assault") {
		runtime::Ptr<SiegeLocation> loc = parseLocation(params); // Java evaluates the arguments left to right
		assault(player, *loc, params.size() < 3 ? 0 : commons::utils::parseInt(params[2]));
	} else
		sendInfo(player);
}

// Java SiegeCommand.java:45-63
void SiegeCommand::listLocations(Player& player) {
	// Java: the locations sorted by id, grouped by (id - 1) / 10 into a LinkedHashMap: the key is monotonic in the id, so the groups are the
	// runs of equal keys of the sorted list
	std::vector<runtime::Ptr<SiegeLocation>> sorted;
	for (const runtime::Ptr<SiegeLocation>& loc : SiegeService::getInstance().getSiegeLocations().values())
		sorted.push_back(loc);
	std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a->getLocationId() < b->getLocationId(); });
	std::vector<std::vector<runtime::Ptr<SiegeLocation>>> locations;
	for (const runtime::Ptr<SiegeLocation>& loc : sorted) {
		if (locations.empty() || (locations.back().front()->getLocationId() - 1) / 10 != (loc->getLocationId() - 1) / 10)
			locations.emplace_back();
		locations.back().push_back(loc);
	}
	for (const std::vector<runtime::Ptr<SiegeLocation>>& siegeLocations : locations) {
		for (size_t i = 0; i < siegeLocations.size(); i++) {
			const runtime::Ptr<SiegeLocation>& loc = siegeLocations[i];
			const std::string worldName = this->worldName(loc->getTemplate()->getWorldId());
			const std::string name = loc->getTemplate()->getL10nId() == 0 ? std::string(xml::enumName(loc->getType())) : loc->getTemplate()->getL10n();
			std::string message = name + " (ID: " + std::to_string(loc->getLocationId()) + ") in " + worldName + " belongs to " +
				std::string(xml::enumName(loc->getRace()));
			const int32_t secondsLeft = SiegeService::getInstance().getRemainingSiegeTimeInSeconds(loc->getLocationId());
			if (secondsLeft > 0)
				message += " (" + std::to_string(secondsLeft / 60) + "m " + std::to_string(secondsLeft % 60) + "s until siege ends)";
			if (i > 0 && loc->getType() == SiegeType::ARTIFACT)
				message = '\t' + message;
			sendInfo(player, message);
		}
	}
}

// Java SiegeCommand.java:65-72
void SiegeCommand::startSiege(Player& player, SiegeLocation& loc) {
	if (SiegeService::getInstance().isSiegeInProgress(loc.getLocationId())) {
		sendInfo(player, "This location is already under siege.");
	} else {
		SiegeService::getInstance().startSiege(loc.getLocationId());
		sendInfo(player, "Started siege at " + getLocationName(loc));
	}
}

// Java SiegeCommand.java:74-81
void SiegeCommand::stopSiege(Player& player, SiegeLocation& loc) {
	if (!SiegeService::getInstance().isSiegeInProgress(loc.getLocationId())) {
		sendInfo(player, "This location is not under siege.");
	} else {
		SiegeService::getInstance().stopSiege(loc.getLocationId());
		sendInfo(player, "Stopped siege at " + getLocationName(loc));
	}
}

// Java SiegeCommand.java:83-110
void SiegeCommand::capture(Player& player, SiegeLocation& loc, std::span<const std::string> params) {
	std::optional<SiegeRace> sr; // Java: null
	runtime::Ptr<Legion> legion;
	if (params.size() >= 3) {
		try {
			sr = utils::enumValueOf<SiegeRace>(commons::utils::StringUtils::toUpperCase(params[2]));
		} catch (const commons::utils::IllegalArgumentException&) {
			try {
				const int32_t legionId = commons::utils::parseInt(params[2]);
				legion = LegionService::getInstance().getLegion(legionId);
			} catch (const commons::utils::NumberFormatException&) {
				std::string legionName;
				for (size_t i = 2; i < params.size(); i++)
					legionName += " " + params[i];
				legion = LegionService::getInstance().getLegion(commons::utils::StringUtils::trim(legionName));
			}
			if (legion) {
				runtime::Ptr<model::team::legion::LegionMember> general = legion->getBrigadeGeneral();
				runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = PlayerService::getOrLoadPlayerCommonData(general->getObjectId());
				if (!pcd)
					throw runtime::NullPointerException("PlayerCommonData of " + std::to_string(general->getObjectId()));
				sr = model::siege::getByRace(pcd->getRace());
			}
		}
		if (!legion && !sr) {
			sendInfo(player, params[2] + " is not valid race or legion");
			return;
		}
	} else {
		sr = model::siege::getByRace(player.getRace());
	}
	// sr is set here: by the race argument, the legion's general or the admin
	SiegeService::getInstance().captureSiege(*sr, legion ? legion->getLegionId() : 0, loc.getLocationId());
}

// Java SiegeCommand.java:112-121
void SiegeCommand::assault(Player& player, SiegeLocation& loc, int32_t delaySeconds) {
	if (BalaurAssaultService::getInstance().startAssault(loc.getLocationId(), delaySeconds))
		sendInfo(player, "Started assault on " + getLocationName(loc));
	else {
		if (SiegeService::getInstance().isSiegeInProgress(loc.getLocationId()))
			sendInfo(player, "Assault on " + getLocationName(loc) + " was already started.");
		else
			sendInfo(player, getLocationName(loc) + " must be under siege in order to start an assault.");
	}
}

// Java SiegeCommand.java:123-128
runtime::Ptr<SiegeLocation> SiegeCommand::parseLocation(std::span<const std::string> params) {
	runtime::Ptr<SiegeLocation> location = params.size() < 2 ? nullptr : SiegeService::getInstance().getSiegeLocation(commons::utils::parseInt(params[1]));
	if (!location)
		throw commons::utils::IllegalArgumentException("Invalid location ID.");
	return location;
}

// Java SiegeCommand.java:130-132. Java's null l10n (name id 0) is the empty string of L10n::getL10n.
std::string SiegeCommand::getLocationName(SiegeLocation& loc) {
	const std::string l10n = loc.getTemplate()->getL10n();
	return l10n.empty() ? std::string(xml::enumName(loc.getType())) + " " + std::to_string(loc.getLocationId()) : l10n;
}

} // namespace aion::gameserver::handlers::admincommands
