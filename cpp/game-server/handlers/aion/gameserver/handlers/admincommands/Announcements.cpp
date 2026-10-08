#include "aion/gameserver/handlers/admincommands/Announcements.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Announcement.h"
#include "aion/gameserver/services/AnnouncementService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Announcements);

/** Java String.replace(CharSequence, CharSequence): every occurrence, left to right */
std::string Announcements::replace(std::string s, std::string_view from, std::string_view to) { // parity: (a C++ helper: Java's String.replace)
	for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) // parity: (a C++ helper: Java's String.replace)
		s.replace(at, from.size(), to); // parity: (a C++ helper: Java's String.replace)
	return s; // parity: (a C++ helper: Java's String.replace)
} // parity: (a C++ helper: Java's String.replace)

Announcements::Announcements()
	: AdminCommand("announcements", "Manages automatic announcements.",
		  "list - Shows all announcements including their ID.\n"
		  "reload - Reloads all announcements from DB.\n"
		  "add <elyos|asmodians|all> <chatType> <delay> <message> - Adds the specified message (delay is in seconds, chatType can be system, "
		  "white, orange, shout or yellow).\n"
		  "delete <id> - Deletes the announcement with the specified ID.\n") {
}

// Java Announcements.java:30-96
void Announcements::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}

	if (params[0] == "list") { // parity= if (params[0].equals("list")) {
		std::vector<runtime::Ptr<model::Announcement>> announcements = services::AnnouncementService::getInstance().getAnnouncements();
		std::string msg;
		if (announcements.empty()) {
			msg = "There are no active announcements.";
		} else {
			msg = "Announcements:";
			for (const runtime::Ptr<model::Announcement>& announce : announcements) {
				msg += "\nID: " + std::to_string(announce->getId()) + " (chat type: " + announce->getType() + ", delay: " + std::to_string(announce->getDelay()) + "s";
				if (announce->getFaction() != std::nullopt) // parity= if (announce.getFaction() != null)
					msg += ", faction: " + std::string(xml::enumName(*announce->getFaction())); // parity= msg += ", faction: " + announce.getFaction();
				msg += ")\n\t\"" + announce->getAnnounce() + "\"";
			}
		}
		sendInfo(player, msg);
	} else if (params[0] == "reload") { // parity= } else if (params[0].equals("reload")) {
		services::AnnouncementService::getInstance().reload();
		sendInfo(player, "Reloaded " + std::to_string(services::AnnouncementService::getInstance().getAnnouncements().size()) + " announcements.");
	} else if (params[0] == "add") { // parity= } else if (params[0].equals("add")) {
		if (params.size() < 4) {
			sendInfo(player);
			return;
		}

		std::string faction = commons::utils::StringUtils::toUpperCase(params[1]);
		constexpr std::array<std::string_view, 3> factions{"ELYOS", "ASMODIANS", "ALL"}; // parity: (the Arrays.asList of the next line)
		if (std::ranges::find(factions, faction) == factions.end()) { // parity= if (!Arrays.asList("ELYOS", "ASMODIANS", "ALL").contains(faction)) {
			sendInfo(player, "Please specify a valid faction parameter.");
			return;
		}

		std::string chatType = commons::utils::StringUtils::toUpperCase(params[2]);
		constexpr std::array<std::string_view, 5> chatTypes{"SYSTEM", "WHITE", "ORANGE", "SHOUT", "YELLOW"}; // parity: (the Arrays.asList of the next line)
		if (std::ranges::find(chatTypes, chatType) == chatTypes.end()) { // parity= if (!Arrays.asList("SYSTEM", "WHITE", "ORANGE", "SHOUT", "YELLOW").contains(chatType)) {
			sendInfo(player, "Please specify a valid chat type parameter.");
			return;
		}

		int32_t delay = commons::utils::parseInt(params[3]);
		if (delay < 300) {
			sendInfo(player, "Delay must be at least 300s (5 minutes).");
			return;
		}

		std::string message = replace(replace(join(params, 4), "\\n", "\n"), "\\t", "\t"); // parity= String message = join(params, 4).replace("\\n", "\n").replace("\\t", "\t");
		if (message.empty()) {
			sendInfo(player, "The message cannot be empty.");
			return;
		}

		if (services::AnnouncementService::getInstance().addAnnouncement(message, faction, chatType, delay))
			sendInfo(player, "The announcement has been created successfully");
		else
			sendInfo(player, "The announcement could not be created");
	} else if (params[0] == "delete") { // parity= } else if (params[0].equals("delete")) {
		if (params.size() < 2) {
			sendInfo(player, "Please specify the ID of the announcement to delete.");
			return;
		}

		int32_t id = commons::utils::parseInt(params[1]);
		// Delete the announcement from the database
		if (services::AnnouncementService::getInstance().delAnnouncement(id))
			sendInfo(player, "The announcement has been deleted successfully.");
		else
			sendInfo(player, "The announcement could not be deleted.");
	} else {
		sendInfo(player);
	}
}

} // namespace aion::gameserver::handlers::admincommands
