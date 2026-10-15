#include "aion/gameserver/handlers/consolecommands/Bookmark_add.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_BOOKMARK_ADD.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Bookmark_add);

// parity= public static final String ALIAS = "Bookmark_add"; (Bookmark_add.h)

// Java Bookmark_add.java:17-19
Bookmark_add::Bookmark_add() : ConsoleCommand(ALIAS) {
}

// Java Bookmark_add.java:21-31
void Bookmark_add::execute(Player& admin, std::span<const std::string> params) {
	std::string bookmarkName = join(params, 0); // parity= String bookmarkName = String.join(" ", params);
	if (bookmarkName.empty())
		return;
	if (commons::utils::StringUtils::utf16Length(bookmarkName) > 27) // parity= if (bookmarkName.length() > 27)
		bookmarkName = commons::utils::StringUtils::substring(bookmarkName, 0, 27);
	runtime::Ref<dao::BookmarkDAO::Bookmark> bookmark = dao::BookmarkDAO::Bookmark::create(bookmarkName, admin.getWorldId(), admin.getX(), admin.getY(), admin.getZ()); // parity= Bookmark bookmark = new Bookmark(bookmarkName, admin.getWorldId(), admin.getX(), admin.getY(), admin.getZ());
	dao::BookmarkDAO::storeBookmark(admin.getObjectId(), *bookmark);
	PacketSendUtility::sendPacket(admin, SM_GM_BOOKMARK_ADD(*bookmark));
}

} // namespace aion::gameserver::handlers::consolecommands
