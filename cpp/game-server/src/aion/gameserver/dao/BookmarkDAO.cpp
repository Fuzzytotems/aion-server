#include "aion/gameserver/dao/BookmarkDAO.h"

#include <bit>
#include <string>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/JavaHash.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

// Java: LoggerFactory.getLogger(BookmarkDAO.class) at each use
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.BookmarkDAO");

namespace {

/** Java record equals of a float component: Float.compare(a, b) == 0 (all NaNs equal, 0.0f != -0.0f) */
bool floatComponentEquals(float a, float b) {
	return detail::javaFloatHashCode(a) == detail::javaFloatHashCode(b);
}

} // namespace

BookmarkDAO::Bookmark::Bookmark(std::string_view name, int32_t worldId, float x, float y, float z)
	: name_(name), worldId_(worldId), x_(x), y_(y), z_(z) {
}

runtime::Ref<BookmarkDAO::Bookmark> BookmarkDAO::Bookmark::create(std::string_view name, int32_t worldId, float x, float y, float z) {
	return runtime::makeRef<Bookmark>(name, worldId, x, y, z);
}

bool BookmarkDAO::Bookmark::equals(const Bookmark& obj) const {
	return name_ == obj.name_ && worldId_ == obj.worldId_ && floatComponentEquals(x_, obj.x_) && floatComponentEquals(y_, obj.y_) &&
		   floatComponentEquals(z_, obj.z_);
}

int32_t BookmarkDAO::Bookmark::hashCode() const {
	// Java record hashCode: 31 * h + hash(component); String.hashCode over UTF-16 code units, Float.hashCode = floatToIntBits
	int32_t h = detail::javaStringHashCode(name_);
	h = detail::combineHash(h, worldId_);
	h = detail::combineHash(h, detail::javaFloatHashCode(x_));
	h = detail::combineHash(h, detail::javaFloatHashCode(y_));
	h = detail::combineHash(h, detail::javaFloatHashCode(z_));
	return h;
}

BookmarkDAO::Bookmark::~Bookmark() = default;

std::vector<runtime::Ref<BookmarkDAO::Bookmark>> BookmarkDAO::loadBookmarks(int32_t playerId) {
	std::vector<runtime::Ref<Bookmark>> bookmarks;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT * FROM `bookmark` where player_id= ?");
		stmt->setInt(1, playerId);
		stmt->execute();
		commons::database::ResultSet* rs = stmt->getResultSet();
		while (rs->next())
			bookmarks.push_back(Bookmark::create(rs->getString("name"), rs->getInt("world_id"), rs->getFloat("x"), rs->getFloat("y"), rs->getFloat("z")));
	} catch (const SQLException& e) {
		log.error("Could not load bookmarks for player: " + std::to_string(playerId), e);
	}
	return bookmarks;
}

void BookmarkDAO::storeBookmark(int32_t playerId, BookmarkDAO::Bookmark& bookmark) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("REPLACE INTO `bookmark` (player_id, name, world_id, x, y, z) VALUES (?, ?, ?, ?, ?, ?)");
		stmt->setInt(1, playerId);
		stmt->setString(2, bookmark.name());
		stmt->setInt(3, bookmark.worldId());
		stmt->setFloat(4, bookmark.x());
		stmt->setFloat(5, bookmark.y());
		stmt->setFloat(6, bookmark.z());
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Could not add bookmark for player " + std::to_string(playerId), e);
	}
}

bool BookmarkDAO::deleteBookmark(int32_t playerId, std::string_view name) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("DELETE FROM `bookmark` WHERE player_id = ? and name = ?");
		stmt->setInt(1, playerId);
		stmt->setString(2, name);
		return stmt->executeUpdate() > 0;
	} catch (const SQLException& e) {
		log.error("Could not delete bookmark " + std::string(name) + " for player " + std::to_string(playerId), e);
		return false;
	}
}

void BookmarkDAO::deleteAll(int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("DELETE FROM `bookmark` WHERE player_id = ?");
		stmt->setInt(1, playerId);
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Could not delete all bookmarks", e);
	}
}

} // namespace aion::gameserver::dao
