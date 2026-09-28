#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

class BookmarkDAO {
public:
	class Bookmark : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const std::string name_{};
		const int32_t worldId_{};
		const float x_{};
		const float y_{};
		const float z_{};
	protected:
		Bookmark(std::string_view name, int32_t worldId, float x, float y, float z); // canonical record constructor
	public:
		static runtime::Ref<BookmarkDAO::Bookmark> create(std::string_view name, int32_t worldId, float x, float y, float z);
		std::string name() const { return this->name_; }
		int32_t worldId() const { return this->worldId_; }
		float x() const { return this->x_; }
		float y() const { return this->y_; }
		float z() const { return this->z_; }
		/** Java record equals: all components */
		bool equals(const Bookmark& obj) const;
		int32_t hashCode() const;
	protected:
		~Bookmark() override;
	};
	static std::vector<runtime::Ref<BookmarkDAO::Bookmark>> loadBookmarks(int32_t playerId);
	static void storeBookmark(int32_t playerId, BookmarkDAO::Bookmark& bookmark);
	static bool deleteBookmark(int32_t playerId, std::string_view name);
	static void deleteAll(int32_t playerId);
};

} // namespace aion::gameserver::dao
