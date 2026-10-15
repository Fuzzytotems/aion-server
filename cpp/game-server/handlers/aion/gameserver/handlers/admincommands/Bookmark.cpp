#include "aion/gameserver/handlers/admincommands/Bookmark.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/handlers/consolecommands/Bookmark_add.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Bookmark);

// Java Bookmark.java:11-17
Bookmark::Bookmark()
	: AdminCommand("bookmark", "Manages teleport bookmarks.",
		  "del <name> - Deletes the bookmark with the specified name.\n"
		  "deleteAll - Deletes all bookmarks.\n"
		  "Note: Press Shift+G and click the \"Bookmark\" button to add or use your teleport bookmarks.\n") {
}

// Java Bookmark.java:19-22
std::string Bookmark::getAliasForLevel() {
	return std::string(consolecommands::Bookmark_add::ALIAS); // parity= return Bookmark_add.ALIAS;
}

// Java Bookmark.java:24-40
void Bookmark::execute(Player& player, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase; // parity: C++ using-declaration
	if (params.size() >= 2 && equalsIgnoreCase(params[0], "del")) { // parity= if (params.length >= 2 && params[0].equalsIgnoreCase("del")) {
		std::string bookmarkName = commons::utils::StringUtils::substring(join(params, 0), 4); // parity= String bookmarkName = String.join(" ", params).substring(4);
		bool deleted = dao::BookmarkDAO::deleteBookmark(player.getObjectId(), bookmarkName);
		if (deleted) {
			sendInfo(player, "The bookmark has been deleted. The bookmarks list will be updated after relog.");
		} else {
			sendInfo(player, "No bookmark with that name was found.");
		}
	} else if (params.size() >= 1 && equalsIgnoreCase(params[0], "deleteAll")) { // parity= } else if (params.length >= 1 && params[0].equalsIgnoreCase("deleteAll")) {
		dao::BookmarkDAO::deleteAll(player.getObjectId());
		sendInfo(player, "All bookmarks have been deleted. The bookmarks list will be updated after relog.");
	} else {
		sendInfo(player);
	}
}

} // namespace aion::gameserver::handlers::admincommands
