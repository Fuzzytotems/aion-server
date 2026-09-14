#include "aion/gameserver/dao/BookmarkDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

BookmarkDAO::Bookmark::Bookmark(std::string_view name, int32_t worldId, float x, float y, float z)
	: name_(name), worldId_(worldId), x_(x), y_(y), z_(z) {
}

runtime::Ref<BookmarkDAO::Bookmark> BookmarkDAO::Bookmark::create(std::string_view name, int32_t worldId, float x, float y, float z) {
	return runtime::makeRef<Bookmark>(name, worldId, x, y, z);
}

bool BookmarkDAO::Bookmark::equals(const Bookmark& obj) const {
	return name_ == obj.name_ && worldId_ == obj.worldId_ && x_ == obj.x_ && y_ == obj.y_ && z_ == obj.z_;
}

int32_t BookmarkDAO::Bookmark::hashCode() const {
	// Java: String.hashCode/Enum identity hash of the components; ported with the first hash collection that holds the record
	AION_UNPORTED();
}

BookmarkDAO::Bookmark::~Bookmark() = default;

std::vector<runtime::Ref<BookmarkDAO::Bookmark>> BookmarkDAO::loadBookmarks(int32_t playerId) {
	AION_UNPORTED();
}

void BookmarkDAO::storeBookmark(int32_t playerId, BookmarkDAO::Bookmark& bookmark) {
	AION_UNPORTED();
}

bool BookmarkDAO::deleteBookmark(int32_t playerId, std::string_view name) {
	AION_UNPORTED();
}

void BookmarkDAO::deleteAll(int32_t playerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
