#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * C++: the header includes BookmarkDAO.h for the nested record type Bookmark (a nested class cannot be forward-declared).
 *
 * @author Yeats
 */
class SM_GM_BOOKMARK_ADD : public AionServerPacket {
private:
	runtime::Ref<dao::BookmarkDAO::Bookmark> bookmark{};

public:
	explicit SM_GM_BOOKMARK_ADD(dao::BookmarkDAO::Bookmark& bookmark);
	~SM_GM_BOOKMARK_ADD() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
