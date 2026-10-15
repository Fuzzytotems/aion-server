// Java-compatible hashes of the DAO records (P4-14): String.hashCode, Float.hashCode, Long.hashCode and the record combiner
// (java.lang.runtime.ObjectMethods: h = 31 * h + hash(component), starting at 0). Expected values are hand-derived from the Java definitions:
// s[0]*31^(n-1) + ... + s[n-1] over UTF-16 code units with int overflow.

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/detail/JavaHash.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::dao {
namespace {

TEST(RecordHashTest, StringHashCodeMatchesJava) {
	EXPECT_EQ(detail::javaStringHashCode(""), 0);
	EXPECT_EQ(detail::javaStringHashCode("a"), 97);
	// "hello": ((((104*31+101)*31+108)*31+108)*31+111 = 99162322
	EXPECT_EQ(detail::javaStringHashCode("hello"), 99162322);
	// "polygenelubricants" is the classic Java string whose hashCode is Integer.MIN_VALUE
	EXPECT_EQ(detail::javaStringHashCode("polygenelubricants"), std::numeric_limits<int32_t>::min());
	// U+00E9 is one UTF-16 code unit (233), not its two UTF-8 bytes
	EXPECT_EQ(detail::javaStringHashCode("\xC3\xA9"), 233);
	// U+1D11E is the surrogate pair D834 DD1E: 0xD834 * 31 + 0xDD1E = 55348 * 31 + 56606 = 1772394
	EXPECT_EQ(detail::javaStringHashCode("\xF0\x9D\x84\x9E"), 1772394);
}

TEST(RecordHashTest, FloatAndLongHashCodesMatchJava) {
	EXPECT_EQ(detail::javaFloatHashCode(0.0f), 0);
	EXPECT_EQ(detail::javaFloatHashCode(-0.0f), std::numeric_limits<int32_t>::min()); // floatToIntBits(-0.0f) = 0x80000000
	EXPECT_EQ(detail::javaFloatHashCode(1.0f), 0x3f800000);
	EXPECT_EQ(detail::javaFloatHashCode(std::numeric_limits<float>::quiet_NaN()), 0x7fc00000);
	EXPECT_EQ(detail::javaLongHashCode(0), 0);
	EXPECT_EQ(detail::javaLongHashCode(1), 1);
	EXPECT_EQ(detail::javaLongHashCode(-1), 0); // 0xFFFFFFFF ^ 0xFFFFFFFF
	EXPECT_EQ(detail::javaLongHashCode(0x100000000LL), 1);
}

TEST(RecordHashTest, BookmarkRecordHashAndEquals) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	auto bookmark = BookmarkDAO::Bookmark::create("a", 7, 1.0f, 0.0f, -0.0f);
	// components: hash("a") = 97, 7, floatToIntBits(1.0f) = 0x3f800000, floatToIntBits(0.0f) = 0, floatToIntBits(-0.0f) = 0x80000000, combined
	// with Java int overflow (computed independently in Python)
	EXPECT_EQ(bookmark->hashCode(), -455469446);
	EXPECT_TRUE(bookmark->equals(*BookmarkDAO::Bookmark::create("a", 7, 1.0f, 0.0f, -0.0f)));
	EXPECT_FALSE(bookmark->equals(*BookmarkDAO::Bookmark::create("a", 7, 1.0f, 0.0f, 0.0f))) << "Float.compare: 0.0f != -0.0f";
	auto nanBookmark = BookmarkDAO::Bookmark::create("a", 7, std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
	EXPECT_TRUE(nanBookmark->equals(*BookmarkDAO::Bookmark::create("a", 7, std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f)))
		<< "Float.compare: NaN equals NaN";
}

TEST(RecordHashTest, RankingListRecordHashes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// RankingListPlayerGp(1, 2, 3): ((1)*31 + 2)*31 + 3 = 1026
	EXPECT_EQ(AbyssRankDAO::RankingListPlayerGp(1, 2, 3).hashCode(), 1026);

	// RankingListLegion(1, 2, 3, "", ELYOS, 4, 5L, 6): hash("") = 0, ELYOS hashes by its ordinal 0, Long.hashCode(5) = 5
	auto legion = AbyssRankDAO::RankingListLegion::create(1, 2, 3, "", model::Race::ELYOS, 4, 5, 6);
	int32_t expected = 1;
	for (int32_t component : {2, 3, 0, 0, 4, 5, 6})
		expected = static_cast<int32_t>(31u * static_cast<uint32_t>(expected) + static_cast<uint32_t>(component));
	EXPECT_EQ(expected, -691258141);
	EXPECT_EQ(legion->hashCode(), expected);

	// RankingListPlayer: string components hash with String.hashCode, enums with their ordinals
	auto player = AbyssRankDAO::RankingListPlayer::create(1, 2, 3, "a", model::Race::ASMODIANS, 5, 6, 7, 8, 9, model::PlayerClass::WARRIOR,
		model::Gender::FEMALE, "b");
	// components 1, 2, 3, hash("a") = 97, ASMODIANS ordinal 1, 5, 6, 7, 8, 9, WARRIOR ordinal 0, FEMALE ordinal 1, hash("b") = 98, combined with
	// h = 31 * h + c in Java int arithmetic (computed independently in Python)
	EXPECT_EQ(player->hashCode(), 836680028);
	EXPECT_TRUE(player->equals(*AbyssRankDAO::RankingListPlayer::create(1, 2, 3, "a", model::Race::ASMODIANS, 5, 6, 7, 8, 9,
		model::PlayerClass::WARRIOR, model::Gender::FEMALE, "b")));
}

TEST(RecordHashTest, PlayerAndLegionInfoHash) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// PlayerAndLegionInfo(10, "ab", 20, DEPUTY): ((10 * 31 + hash("ab") = 3105) * 31 + 20) * 31 + ordinal 1
	PlayerDAO::PlayerAndLegionInfo info(10, "ab", 20, model::team::legion::LegionRank::DEPUTY);
	EXPECT_EQ(info.hashCode(), static_cast<int32_t>(((10 * 31 + 3105) * 31 + 20) * 31 + 1));
	EXPECT_EQ(info.hashCode(), 3282436);
	// a player without a legion: the null rank hashes to 0 (Objects.hashCode(null)), header request dao-2
	PlayerDAO::PlayerAndLegionInfo noLegion(10, "ab", 20, std::nullopt);
	EXPECT_EQ(noLegion.hashCode(), 3282435);
	EXPECT_FALSE(noLegion.equals(info));
}

} // namespace
} // namespace aion::gameserver::dao
