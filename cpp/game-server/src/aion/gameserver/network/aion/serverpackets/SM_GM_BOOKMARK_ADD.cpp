#include "aion/gameserver/network/aion/serverpackets/SM_GM_BOOKMARK_ADD.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_BOOKMARK_ADD::SM_GM_BOOKMARK_ADD(dao::BookmarkDAO::Bookmark& bookmarkValue)
	: AionServerPacket(opcodeOf<SM_GM_BOOKMARK_ADD>), bookmark(bookmarkValue) {
}

SM_GM_BOOKMARK_ADD::~SM_GM_BOOKMARK_ADD() = default;

void SM_GM_BOOKMARK_ADD::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
