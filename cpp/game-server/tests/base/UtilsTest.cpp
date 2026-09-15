// P4-05 utilities with hand-derived expectations: the SplitList family, CollectionUtil, Chance, TimeUtil, Util, ChatUtil, ServerTime,
// CompressUtil (zlib vectors from Python's zlib module), DDSConverter, CAPTCHAUtil words, XmlUtil and HTMLCache.

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/ZipWriter.h"
#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/HTMLConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/model/Chance.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/TimeUtil.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/utils/captcha/CAPTCHAUtil.h"
#include "aion/gameserver/utils/captcha/DDSConverter.h"
#include "aion/gameserver/utils/collections/CollectionUtil.h"
#include "aion/gameserver/utils/collections/DynamicElementCountSplitList.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"
#include "aion/gameserver/utils/collections/FixedElementCountSplitList.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/utils/xml/CompressUtil.h"
#include "aion/gameserver/utils/xml/XmlUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::utils {
namespace {

// ------------------------------------------------------------------------------------------------------------------------------ SplitList

std::vector<int32_t> partSizes(collections::SplitList<int32_t>& list) {
	std::vector<int32_t> sizes;
	for (collections::ListPart<int32_t>& part : list)
		sizes.push_back(part.size());
	return sizes;
}

TEST(SplitListTest, FixedElementCount) {
	collections::FixedElementCountSplitList<int32_t> list({1, 2, 3, 4, 5, 6, 7}, false, 3);
	EXPECT_EQ(partSizes(list), (std::vector<int32_t>{3, 3, 1}));
	auto it = list.iterator();
	ASSERT_TRUE(it.hasNext());
	collections::ListPart<int32_t>& first = it.next();
	EXPECT_TRUE(first.isFirst());
	EXPECT_FALSE(first.isLast());
	EXPECT_EQ(first.getPartNo(), 1);
	EXPECT_EQ(first.get(2), 3);
	collections::ListPart<int32_t>& second = it.next();
	EXPECT_FALSE(second.isFirst());
	collections::ListPart<int32_t>& third = it.next();
	EXPECT_TRUE(third.isLast());
	EXPECT_EQ(third.get(0), 7);
	EXPECT_THROW(third.get(1), runtime::IndexOutOfBoundsException);
	EXPECT_FALSE(it.hasNext());
	EXPECT_THROW(it.next(), runtime::NoSuchElementException);

	EXPECT_THROW(collections::FixedElementCountSplitList<int32_t>({1}, false, 0), runtime::IllegalArgumentException);
}

TEST(SplitListTest, EmptyLists) {
	collections::FixedElementCountSplitList<int32_t> none({}, false, 3);
	EXPECT_TRUE(partSizes(none).empty());
	collections::FixedElementCountSplitList<int32_t> one({}, true, 3);
	auto it = one.iterator();
	ASSERT_TRUE(it.hasNext());
	collections::ListPart<int32_t>& part = it.next();
	EXPECT_TRUE(part.isEmpty());
	EXPECT_TRUE(part.isFirst());
	EXPECT_TRUE(part.isLast());
}

TEST(SplitListTest, DynamicElementCount) {
	runtime::PinnedCallback<int32_t(int32_t&)> length([](int32_t& value) { return value; });
	collections::DynamicElementCountSplitList<int32_t> list({4, 3, 3, 5, 1, 1, 10}, false, 10, length);
	EXPECT_EQ(partSizes(list), (std::vector<int32_t>{3, 3, 1})); // 4+3+3, 5+1+1, 10
	collections::DynamicElementCountSplitList<int32_t> tooLong({4, 11}, false, 10, length);
	try {
		partSizes(tooLong);
		FAIL() << "expected IllegalStateException";
	} catch (const runtime::IllegalStateException& e) {
		EXPECT_STREQ(e.what(), "elementLength(11) is greater than the maxLength (10)");
	}
	collections::DynamicElementCountSplitList<int32_t> negative({-1}, false, 10, length);
	EXPECT_THROW(partSizes(negative), runtime::IllegalStateException);
	EXPECT_THROW(collections::DynamicElementCountSplitList<int32_t>({1}, false, 0, length), runtime::IllegalArgumentException);

	// the maximum length is MAX_USABLE_PACKET_BODY_SIZE - 7, so both small elements fit into one part
	collections::DynamicServerPacketBodySplitList<int32_t> packets({1, 2}, true, 7, length);
	EXPECT_EQ(partSizes(packets), (std::vector<int32_t>{2}));
}

// ------------------------------------------------------------------------------------------------------------------ CollectionUtil, Chance

TEST(CollectionUtilTest, ForEachContinuesAfterExceptions) {
	std::vector<int32_t> values{1, 2, 3};
	std::vector<int32_t> visited;
	collections::CollectionUtil::forEach(values, [&visited](int32_t value) {
		visited.push_back(value);
		if (value == 2)
			throw std::runtime_error("expected");
	});
	EXPECT_EQ(visited, values);
	int32_t detailsCalls = 0;
	collections::CollectionUtil::forEach(
		values, [](int32_t) { throw runtime::IllegalStateException("expected"); },
		[&detailsCalls] {
			++detailsCalls;
			return std::string("details");
		});
	EXPECT_EQ(detailsCalls, 3);
}

struct TestChance final : model::Chance {
	explicit TestChance(float chanceValue) : chance(chanceValue) {}
	float getChance() const override { return chance; }
	float chance;
};

TEST(ChanceTest, SelectElement) {
	commons::utils::Rnd::seedCurrentThreadForTests(4805);
	TestChance zero(0);
	TestChance full(100);
	std::vector<const TestChance*> nothing{&zero};
	EXPECT_EQ(model::Chance::selectElement(nothing), nullptr);
	std::vector<const TestChance*> elements{&zero, &full};
	EXPECT_EQ(model::Chance::selectElement(elements), &full);
	EXPECT_EQ(elements.size(), 2u);
	EXPECT_EQ(model::Chance::selectElement(elements, true), &full);
	EXPECT_EQ(elements, (std::vector<const TestChance*>{&zero}));
}

// --------------------------------------------------------------------------------------------------------------------------- TimeUtil, Util

TEST(TimeUtilTest, IsExpired) {
	int64_t now = commons::utils::currentTimeMillis();
	EXPECT_TRUE(TimeUtil::isExpired(now - 1000));
	EXPECT_FALSE(TimeUtil::isExpired(now + 60000));
}

TEST(JavaMathTest, RoundLikeJava) {
	// Math.round(float): ties towards positive infinity, NaN 0, saturating (values from the JDK bit algorithm, hand-checked)
	EXPECT_EQ(JavaMath::round(-2.5f), -2);
	EXPECT_EQ(JavaMath::round(2.5f), 3);
	EXPECT_EQ(JavaMath::round(-1.5f), -1);
	EXPECT_EQ(JavaMath::round(1.5f), 2);
	EXPECT_EQ(JavaMath::round(-0.5f), 0);
	EXPECT_EQ(JavaMath::round(0.5f), 1);
	EXPECT_EQ(JavaMath::round(0.49999997f), 0) << "floor(x + 0.5) would give 1";
	EXPECT_EQ(JavaMath::round(-0.49999997f), 0);
	EXPECT_EQ(JavaMath::round(7.8125f), 8);
	EXPECT_EQ(JavaMath::round(8388609.0f), 8388609);
	EXPECT_EQ(JavaMath::round(-8388609.5f), -8388610) << "the float is -8388610";
	EXPECT_EQ(JavaMath::round(1e-11f), 0);
	EXPECT_EQ(JavaMath::round(std::numeric_limits<float>::quiet_NaN()), 0);
	EXPECT_EQ(JavaMath::round(3e9f), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(JavaMath::round(-3e9f), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(JavaMath::round(std::numeric_limits<float>::infinity()), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(JavaMath::round(-std::numeric_limits<float>::infinity()), std::numeric_limits<int32_t>::min());
	// Math.round(double)
	EXPECT_EQ(JavaMath::round(-2.5), -2LL);
	EXPECT_EQ(JavaMath::round(2.5), 3LL);
	EXPECT_EQ(JavaMath::round(0.49999999999999994), 0LL) << "floor(x + 0.5) would give 1";
	EXPECT_EQ(JavaMath::round(4503599627370497.0), 4503599627370497LL);
	EXPECT_EQ(JavaMath::round(std::numeric_limits<double>::quiet_NaN()), 0LL);
	EXPECT_EQ(JavaMath::round(1e19), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(JavaMath::round(-1e19), std::numeric_limits<int64_t>::min());
	static_assert(JavaMath::round(2.5f) == 3);
}

TEST(UtilTest, ConvertName) {
	configs::main::NameConfig::ALLOW_CUSTOM_NAMES.store(false);
	EXPECT_EQ(Util::convertName("atracer"), "Atracer");
	EXPECT_EQ(Util::convertName("aTRACER"), "Atracer");
	EXPECT_EQ(Util::convertName("\xC3\xA4" "BC"), "\xC3\x84" "bc"); // "äBC" -> "Äbc"
	EXPECT_EQ(Util::convertName(""), "");
	configs::main::NameConfig::ALLOW_CUSTOM_NAMES.store(true);
	EXPECT_EQ(Util::convertName("aTRACER"), "aTRACER");
	configs::main::NameConfig::ALLOW_CUSTOM_NAMES.store(false);
}

// ------------------------------------------------------------------------------------------------------------------------------- ChatUtil

TEST(ChatUtilTest, ColorsUseDecimalFormat) {
	EXPECT_EQ(ChatUtil::color("msg", std::optional<JavaColor>()), "[color:msg;1 1 1]");
	EXPECT_EQ(ChatUtil::color("msg", JavaColor::ORANGE), "[color:msg;1 .78 0]"); // 200 / 255f = 0.78431374
	EXPECT_EQ(ChatUtil::color("msg", 0x80FF00), "[color:msg;.5 1 0]");         // 128 / 255f = 0.5019608
	EXPECT_EQ(ChatUtil::color("msg", 3, 128, 255), "[color:msg;.01 .5 1]");    // 3 / 255f = 0.011764706
	EXPECT_EQ(ChatUtil::position("Teleporter", 400010000, 2128.8f, 1924.3f, 0.0f), "[pos:Teleporter;400010000 2128.8 1924.3 0 1]");
	EXPECT_EQ(ChatUtil::position("Core", 400010000, 2000.125f, 2000.375f, 2000.0f), "[pos:Core;400010000 2000.12 2000.38 2000 2]"); // HALF_EVEN
	EXPECT_EQ(ChatUtil::position("Upper", 400010000, -0.001f, 1.999f, 2500.0f), "[pos:Upper;400010000 -0 2 2500 3]");
	EXPECT_EQ(ChatUtil::position("Poeta", 210010000, 1.0f, 2.0f, 3.0f), "[pos:Poeta;210010000 1 2 3 0]");
}

TEST(ChatUtilTest, LinksAndIds) {
	EXPECT_EQ(ChatUtil::genderize("he", "she"), "he[f:\"she\"]");
	EXPECT_EQ(ChatUtil::path("Mune"), "[where:Mune]");
	EXPECT_EQ(ChatUtil::path("Mune", 203060), "[where:Mune;203060]");
	EXPECT_EQ(ChatUtil::item(182400001), "[item:182400001]");
	EXPECT_EQ(ChatUtil::itemName(182400001), "[item_ex:182400001]");
	EXPECT_EQ(ChatUtil::recipe(155000001), "[recipe:155000001]");
	EXPECT_EQ(ChatUtil::quest(1006), "[quest:1006]");
	EXPECT_EQ(ChatUtil::getItemId("[item: 100000094]"), 100000094);
	EXPECT_EQ(ChatUtil::getItemId("100000094"), 100000094);
	EXPECT_EQ(ChatUtil::getItemId("100000094;ver1]"), 100000094);
	EXPECT_EQ(ChatUtil::getItemId("[item:100000094;ver1]"), 100000094);
	EXPECT_EQ(ChatUtil::getItemId("1000000941"), 0);
	EXPECT_EQ(ChatUtil::getItemId("200000094"), 0);
	EXPECT_EQ(ChatUtil::getItemId(std::nullopt), 0);
	EXPECT_EQ(ChatUtil::getQuestId("[quest:1006]"), 1006);
	EXPECT_EQ(ChatUtil::getQuestId("80020"), 80020);
	EXPECT_EQ(ChatUtil::getQuestId("123"), 0);
	EXPECT_EQ(ChatUtil::leftPad(42, 5), "\t\t\t42");
	EXPECT_EQ(ChatUtil::leftPad(123456, 3), "123456");
	EXPECT_EQ(ChatUtil::l10n(0), "");
	// 1400053 * 2 + 1 = 2800107 = 0x2AB9EB: the UTF-16 units 0xB9EB and 0x002A
	EXPECT_EQ(ChatUtil::l10n(1400053), "$" + commons::utils::StringUtils::toUtf8(std::u16string{u'\xB9EB', u'\x002A'}));
	// 27647 * 2 + 1 = 0xD7FF: the last low word below the surrogate range
	EXPECT_EQ(ChatUtil::l10n(27647), std::string("$\xED\x9F\xBF\0", 5));
	// Known loss (open issue, docs/deviations/P4-05.md): 27648 * 2 + 1 = 0xD801 is a lone surrogate. Java writes the unit unchanged, the UTF-8
	// string holds U+FFFD, so the client would read 0xFFFD. Ids with id % 32768 in 27648..28671 are affected until strings can carry WTF-8.
	EXPECT_EQ(ChatUtil::l10n(27648), std::string("$\xEF\xBF\xBD\0", 5));
	EXPECT_EQ(ChatUtil::l10n(28671), std::string("$\xEF\xBF\xBD\0", 5)); // 0xDFFF
	EXPECT_EQ(ChatUtil::l10n(28672), std::string("$\xEE\x80\x81\0", 5)); // 0xE001
	EXPECT_EQ(ChatUtil::getPosition("no link"), nullptr);
	EXPECT_EQ(ChatUtil::getPosition(std::nullopt), nullptr);
	EXPECT_EQ(ChatUtil::getPosition("[pos:Label]"), nullptr); // no ';'
	// java-bug kept: the parsed text starts with the ';', so every complete link fails in Integer.parseInt
	EXPECT_THROW(ChatUtil::getPosition("[pos:Teleporter;0 400010000 2128.8 1924.3 0.0 2]"), runtime::IllegalArgumentException);
}

TEST(ChatUtilTest, RealCharNames) {
	configs::main::NameConfig::ALLOW_CUSTOM_NAMES.store(false);
	configs::administration::AdminConfig::NAME_TAGS.set({"%s", "[GM] %s", "%s <Dev>"});
	EXPECT_EQ(ChatUtil::getRealCharName("bob"), "Bob");
	EXPECT_EQ(ChatUtil::getRealCharName("[GM] bob"), "Bob");
	EXPECT_EQ(ChatUtil::getRealCharName("\xEE\x81\x93[GM] alice"), "Alice"); // asmodian name prefix U+E053
	// java-bug kept: the end index is one before the suffix, so the last character of the name is cut as well
	EXPECT_EQ(ChatUtil::getRealCharName("carol <Dev>"), "Caro");
	EXPECT_THROW(ChatUtil::getRealCharName(""), runtime::IndexOutOfBoundsException);
	configs::administration::AdminConfig::NAME_TAGS.set({"\xE2\x98\x85 %s"}); // "★ %s": the client sends "?" for the unsupported star
	EXPECT_EQ(ChatUtil::getRealCharName("? dave", true), "Dave");
	EXPECT_EQ(ChatUtil::getRealCharName("? dave", false), "? dave");
	// a supplementary character (U+1F600, a surrogate pair) is one regex match in Java: one "?", not two
	configs::administration::AdminConfig::NAME_TAGS.set({"\xF0\x9F\x98\x80 %s"});
	EXPECT_EQ(ChatUtil::getRealCharName("? erin", true), "Erin");
	EXPECT_EQ(ChatUtil::getRealCharName("?? erin", true), "?? erin");
	configs::administration::AdminConfig::NAME_TAGS.set({});
}

TEST(ChatUtilTest, SplitLongMessages) {
	EXPECT_EQ(ChatUtil::split("short"), (std::vector<std::string>{"short"}));
	std::string words;
	for (int i = 0; i < 300; ++i)
		words += "word ";
	std::vector<std::string> parts = ChatUtil::split(words); // 1500 chars: split at the last space before 1022 characters
	ASSERT_EQ(parts.size(), 2u);
	EXPECT_EQ(parts[0].size(), 1019u); // the space at index 1019 is the last one before the limit
	EXPECT_EQ(parts[0] + " " + parts[1], words);
	std::string links;
	for (int i = 0; i < 40; ++i)
		links += "[item:182400001] "; // estimated as 31 for the link and 1 for the space
	// 680 characters, but 32 links reach the estimate of 1022: the split is at the space after the 31st link (index 31 * 17 - 1)
	std::vector<std::string> linkParts = ChatUtil::split(links);
	ASSERT_EQ(linkParts.size(), 2u);
	EXPECT_EQ(linkParts[0].size(), 526u);
	EXPECT_EQ(linkParts[1].size(), 9u * 17u);
}

// ----------------------------------------------------------------------------------------------------------------------------- ServerTime

class TimeZoneScope {
public:
	explicit TimeZoneScope(const char* zoneName) : previous(configs::main::GSConfig::TIME_ZONE_ID.load()) {
		configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone(zoneName));
	}
	~TimeZoneScope() { configs::main::GSConfig::TIME_ZONE_ID.store(previous); }

private:
	const std::chrono::time_zone* previous;
};

std::chrono::sys_time<std::chrono::milliseconds> utc(int y, unsigned m, unsigned d, int h, int min) {
	using namespace std::chrono;
	return sys_days{year{y} / month{m} / day{d}} + hours{h} + minutes{min};
}

TEST(ServerTimeTest, ZoneConversions) {
	using namespace std::chrono;
	{
		TimeZoneScope zone("Europe/Berlin");
		time::ServerTime::ZonedDateTime winter = time::ServerTime::parseLocal("2025-01-15T10:00");
		EXPECT_EQ(winter.get_sys_time(), utc(2025, 1, 15, 9, 0));
		// daylight saving gap: 02:30 does not exist and is moved forward by the gap (Java ZonedDateTime.of)
		time::ServerTime::ZonedDateTime gap = time::ServerTime::parseLocal("2025-03-30T02:30:00");
		EXPECT_EQ(gap.get_sys_time(), utc(2025, 3, 30, 1, 30));
		EXPECT_EQ(gap.get_local_time(), local_days{year{2025} / March / 30} + hours{3} + minutes{30});
		// overlap: the earlier offset, or the parsed one when it is valid
		EXPECT_EQ(time::ServerTime::parseLocal("2025-10-26T02:30").get_sys_time(), utc(2025, 10, 26, 0, 30));
		EXPECT_EQ(time::ServerTime::parse("2025-10-26T02:30:00+01:00[Europe/Berlin]").get_sys_time(), utc(2025, 10, 26, 1, 30));
		EXPECT_EQ(time::ServerTime::parse("2025-10-26T02:30:00+02:00[Europe/Berlin]").get_sys_time(), utc(2025, 10, 26, 0, 30));
		// Java Parsed.resolveInstant: the parsed offset has priority over the region, even when it is not valid there (Berlin is +02:00 in June)
		EXPECT_EQ(time::ServerTime::parse("2024-06-01T12:00+05:00[Europe/Berlin]").get_sys_time(), utc(2024, 6, 1, 7, 0));
		EXPECT_EQ(time::ServerTime::parse("2025-03-30T02:30+01:00[Europe/Berlin]").get_sys_time(), utc(2025, 3, 30, 1, 30)); // in the gap
		EXPECT_EQ(time::ServerTime::parse("2025-01-15T10:00Z[America/New_York]").get_sys_time(), utc(2025, 1, 15, 10, 0));
		EXPECT_EQ(time::ServerTime::parse("2025-01-15T10:00:00.250Z").get_sys_time(), utc(2025, 1, 15, 10, 0) + milliseconds(250));
		EXPECT_EQ(time::ServerTime::parse("2025-01-15T10:00-05:30").get_sys_time(), utc(2025, 1, 15, 15, 30));
		EXPECT_EQ(time::ServerTime::ofEpochSecond(86400).get_sys_time(), sys_time<milliseconds>(hours(24)));
		EXPECT_EQ(time::ServerTime::ofEpochMilli(1500).get_sys_time(), sys_time<milliseconds>(milliseconds(1500)));
		EXPECT_EQ(time::ServerTime::atDate(sys_time<milliseconds>(milliseconds(7))).get_sys_time(), sys_time<milliseconds>(milliseconds(7)));
		EXPECT_EQ(time::ServerTime::getStandardOffset(), 3600);
		EXPECT_TRUE(time::ServerTime::getOffset() == 3600 || time::ServerTime::getOffset() == 7200);
		EXPECT_EQ(time::ServerTime::getOffset() - time::ServerTime::getDaylightSavings(), 3600);
		EXPECT_THROW(time::ServerTime::parseLocal("2025-02-30T10:00"), runtime::IllegalArgumentException);
		EXPECT_THROW(time::ServerTime::parseLocal("2025-01-15 10:00"), runtime::IllegalArgumentException);
		EXPECT_THROW(time::ServerTime::parse("2025-01-15T10:00"), runtime::IllegalArgumentException);
		EXPECT_THROW(time::ServerTime::parse("2025-01-15T10:00Z[Nowhere/City]"), runtime::IllegalArgumentException);
	}
	TimeZoneScope unset("UTC");
	configs::main::GSConfig::TIME_ZONE_ID.store(nullptr);
	EXPECT_THROW(time::ServerTime::now(), runtime::NullPointerException);
}

// ---------------------------------------------------------------------------------------------------------------------------- CompressUtil

TEST(CompressUtilTest, DecompressesZlibStreams) {
	// Python: zlib.compress(b"hello", 6) - a fixed Huffman block
	std::array<uint8_t, 13> hello{0x78, 0x9C, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x07, 0x00, 0x06, 0x2C, 0x02, 0x15};
	std::vector<uint8_t> helloBytes = xml::CompressUtil::decompress(hello);
	EXPECT_EQ(std::string(helloBytes.begin(), helloBytes.end()), "hello");
	// Python: zlib.compress(b"stored!", 0) - a stored block
	std::array<uint8_t, 18> stored{0x78, 0x01, 0x01, 0x07, 0x00, 0xF8, 0xFF, 0x73, 0x74, 0x6F, 0x72, 0x65, 0x64, 0x21, 0x0B, 0xEF, 0x02, 0xB3};
	std::vector<uint8_t> storedBytes = xml::CompressUtil::decompress(stored);
	EXPECT_EQ(std::string(storedBytes.begin(), storedBytes.end()), "stored!");
	// Python: zlib.compress(text, 9) of 120 random words - a dynamic Huffman block
	std::string text =
		"zone quest legion elyos npc spawn kinah script spawn zone aion npc script item npc spawn legion legion spawn item spawn script legion npc "
		"kinah aion spawn item elyos elyos aion npc aion aion legion npc item npc script kinah quest skill legion quest script spawn aion skill "
		"script kinah elyos quest spawn aion aion elyos item zone spawn script asmodian spawn aion npc aion item house elyos script legion balaur "
		"zone house aion house zone skill item balaur quest asmodian balaur item spawn aion skill script house zone asmodian house skill aion "
		"spawn spawn script legion quest balaur zone quest house legion npc elyos spawn balaur script aion balaur kinah zone zone asmodian zone "
		"aion house aion balaur house";
	std::array<uint8_t, 215> dynamic{
		0x78, 0xDA, 0x6D, 0x52, 0xED, 0x0E, 0x83, 0x20, 0x0C, 0x7C, 0x15, 0x5E, 0xAD, 0x73, 0x64, 0x12, 0x11, 0x9C, 0xC5, 0x2C, 0xF3, 0xE9, 0x17,
		0x7A, 0x15, 0xCA, 0xB6, 0x3F, 0x4A, 0xCE, 0xFB, 0x68, 0x0F, 0xCF, 0x9C, 0xBC, 0x7B, 0x1E, 0x9E, 0x8B, 0x8B, 0xFE, 0x11, 0x72, 0x72, 0x3E,
		0xBE, 0x33, 0xBB, 0xB4, 0x4D, 0x8E, 0x37, 0x7A, 0x25, 0xB7, 0x84, 0x44, 0xB3, 0xE3, 0x69, 0x0F, 0x5B, 0x51, 0xE8, 0xAC, 0x22, 0xAA, 0x64,
		0xA1, 0xE1, 0x53, 0x28, 0x7E, 0x35, 0x32, 0x75, 0xD3, 0x17, 0x30, 0xA1, 0xE0, 0xA8, 0x22, 0xFD, 0x5C, 0x65, 0xC8, 0xA1, 0x2F, 0x36, 0xA6,
		0xC1, 0xB3, 0x25, 0xCA, 0x81, 0x8C, 0x7D, 0x05, 0x7B, 0x3E, 0xAC, 0xE1, 0x87, 0xD5, 0x78, 0x09, 0x31, 0x5E, 0x64, 0x85, 0xEC, 0x42, 0x48,
		0x15, 0xD2, 0xA0, 0x46, 0xAE, 0x0A, 0x3A, 0x93, 0x7A, 0x4F, 0x92, 0x2A, 0x7D, 0x0C, 0x7B, 0x11, 0xAF, 0xF9, 0x1E, 0x28, 0x59, 0x55, 0x9B,
		0x5C, 0x34, 0x73, 0x3E, 0xD8, 0xAB, 0xC9, 0x58, 0xC6, 0x8D, 0x22, 0x1D, 0x3B, 0x4C, 0xC1, 0x12, 0x15, 0x8E, 0x88, 0x92, 0x49, 0xC5, 0x46,
		0xC9, 0x18, 0xB1, 0xA5, 0x2A, 0x6A, 0xFA, 0xFE, 0xDD, 0xD0, 0xF8, 0x35, 0x1D, 0x30, 0xD0, 0xCC, 0x4D, 0xFC, 0xBB, 0x32, 0x24, 0xDA, 0x59,
		0x81, 0xC0, 0xC2, 0xDC, 0x8B, 0xAE, 0x28, 0x16, 0x4A, 0xBF, 0x4A, 0x32, 0xDB, 0xA2, 0x6F, 0xF1, 0x19, 0x47, 0xEA, 0xFF, 0x9A, 0xE9, 0x42,
		0x45, 0x82, 0x7C, 0x00, 0x6B, 0x77, 0x07, 0x81};
	std::vector<uint8_t> dynamicBytes = xml::CompressUtil::decompress(dynamic);
	EXPECT_EQ(std::string(dynamicBytes.begin(), dynamicBytes.end()), text);
}

TEST(CompressUtilTest, RoundTripAndErrors) {
	std::string script = "function OnInit() return 1 end -- aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	std::span<const uint8_t> scriptBytes(reinterpret_cast<const uint8_t*>(script.data()), script.size());
	std::vector<uint8_t> compressed = xml::CompressUtil::compress(scriptBytes);
	EXPECT_EQ(compressed[0], 0x78);
	EXPECT_EQ(compressed[1], 0x9C);
	EXPECT_LT(compressed.size(), script.size());
	std::vector<uint8_t> restored = xml::CompressUtil::decompress(compressed);
	EXPECT_EQ(std::string(restored.begin(), restored.end()), script);
	EXPECT_EQ(xml::CompressUtil::decompress(xml::CompressUtil::compress({})).size(), 0u);

	std::vector<uint8_t> truncated(compressed.begin(), compressed.end() - 3);
	try {
		xml::CompressUtil::decompress(truncated);
		FAIL() << "expected IllegalStateException";
	} catch (const runtime::IllegalStateException& e) {
		EXPECT_EQ(std::string(e.what()), "Bad zip data, size: " + std::to_string(truncated.size()));
	}
	std::vector<uint8_t> badChecksum = compressed;
	badChecksum.back() ^= 1;
	EXPECT_THROW(xml::CompressUtil::decompress(badChecksum), runtime::IllegalArgumentException);
	std::array<uint8_t, 3> badHeader{0x12, 0x34, 0x56};
	EXPECT_THROW(xml::CompressUtil::decompress(badHeader), runtime::IllegalArgumentException);
	EXPECT_THROW(xml::CompressUtil::decompress({}), runtime::IllegalStateException);

	auto dataFormatMessage = [](std::vector<uint8_t> bytes) {
		try {
			xml::CompressUtil::decompress(bytes);
		} catch (const runtime::IllegalArgumentException& e) {
			return std::string(e.what());
		}
		return std::string("no exception");
	};
	// zlib inflate checks the header in this order: check bits, method, window size
	EXPECT_EQ(dataFormatMessage({0x78, 0x9D, 0x03, 0x00}), "incorrect header check");
	EXPECT_EQ(dataFormatMessage({0x77, 0x09, 0x03, 0x00}), "unknown compression method"); // 0x7709 % 31 == 0, method 7
	EXPECT_EQ(dataFormatMessage({0x88, 0x1C, 0x03, 0x00}), "invalid window size");         // 0x881C % 31 == 0, 2^16 window
	EXPECT_EQ(dataFormatMessage({0x78, 0x9C, 0x07, 0x00}), "invalid block type");          // BFINAL 1, BTYPE 3
	EXPECT_EQ(dataFormatMessage({0x78, 0x9C, 0x01, 0x05, 0x00, 0x00, 0x00}), "invalid stored block lengths");
	std::vector<uint8_t> trailing = xml::CompressUtil::compress(std::span<const uint8_t>(scriptBytes));
	trailing.push_back(0xAB); // bytes after the stream end are ignored: Inflater.finished() ends the loop
	EXPECT_EQ(xml::CompressUtil::decompress(trailing).size(), script.size());
}

/** the generators of the Python oracle (zlib.compress(data, 6) of zlib 1.3.1): Numerical Recipes LCG, 24 high bits */
class Lcg {
public:
	explicit Lcg(uint32_t seed) : state(seed) {}
	uint32_t next() {
		state = state * 1664525u + 1013904223u;
		return state >> 8;
	}

private:
	uint32_t state;
};

std::vector<uint8_t> oracleTextInput(uint32_t seed, size_t units) {
	static const std::array<std::string_view, 24> WORDS{"house", "script", "<item id=\"", "\"/>", "</object>", "<object name=\"", "Elyos",
		"Asmodian", " ", "\n\t", "value", "0", "1", "2", "Pandaemonium", "Sanctum", "lua", "function", "end", "return", "local", "if", "then",
		"else"};
	Lcg lcg(seed);
	std::string text;
	while (text.size() < units) {
		uint32_t r = lcg.next();
		if (r % 97 == 0)
			text.append(200 + r % 300, 'a'); // long runs: matches of 258
		else
			text += WORDS[r % WORDS.size()];
	}
	text.resize(units);
	std::vector<uint8_t> utf16le;
	for (char c : text) {
		utf16le.push_back(static_cast<uint8_t>(c));
		utf16le.push_back(0);
	}
	return utf16le;
}

std::vector<uint8_t> oracleRandomInput(uint32_t seed, size_t size) {
	Lcg lcg(seed);
	std::vector<uint8_t> bytes(size);
	for (uint8_t& b : bytes)
		b = static_cast<uint8_t>(lcg.next());
	return bytes;
}

/** symbol i appears fib(i + 1) times, shuffled: skewed frequencies need code lengths over 15 bits, so zlib's gen_bitlen limits them */
std::vector<uint8_t> oracleFibonacciInput(uint32_t seed) {
	std::vector<uint8_t> bytes;
	uint32_t a = 1;
	uint32_t b = 1;
	for (uint32_t symbol = 0; symbol < 26; ++symbol) {
		bytes.insert(bytes.end(), a, static_cast<uint8_t>(symbol * 7 + 3));
		uint32_t next = a + b;
		a = b;
		b = next;
	}
	Lcg lcg(seed);
	for (size_t i = bytes.size() - 1; i > 0; --i)
		std::swap(bytes[i], bytes[lcg.next() % (i + 1)]);
	return bytes;
}

uint32_t crc32Of(std::span<const uint8_t> bytes) {
	commons::utils::Crc32 crc;
	crc.update(bytes);
	return crc.getValue();
}

TEST(CompressUtilTest, CompressMatchesJavaDeflaterBytes) {
	// java.util.zip.Deflater() is zlib at level 6; expected bytes from Python's zlib.compress(data, 6) (zlib 1.3.1)
	EXPECT_EQ(xml::CompressUtil::compress({}), (std::vector<uint8_t>{0x78, 0x9C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01}));
	std::string hello = "hello";
	EXPECT_EQ(xml::CompressUtil::compress(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(hello.data()), hello.size())),
		(std::vector<uint8_t>{0x78, 0x9C, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x07, 0x00, 0x06, 0x2C, 0x02, 0x15}));

	std::vector<uint8_t> small = oracleTextInput(7, 600); // one dynamic block
	ASSERT_EQ(crc32Of(small), 0xAD2A0B89u) << "the generator differs from the Python oracle";
	std::vector<uint8_t> smallExpected{0x78, 0x9C, 0xED, 0x54, 0x5B, 0x0A, 0xC2, 0x30, 0x10, 0x9C, 0xA3, 0x48, 0x2F, 0x60, 0xEA, 0x77, 0xF5, 0xCF,
		0x7F, 0xC1, 0x13, 0xC4, 0xB4, 0x62, 0x24, 0x6D, 0xC5, 0x34, 0x05, 0x6F, 0xEF, 0x74, 0x2B, 0x24, 0xC1, 0x7A, 0x01, 0x29, 0x0B, 0xD9, 0xDD,
		0xD9, 0xD7, 0x24, 0xB0, 0xB1, 0xB8, 0xC2, 0xC3, 0xE0, 0x09, 0x8B, 0x07, 0x06, 0x38, 0x04, 0x68, 0xDC, 0xD0, 0x53, 0x7B, 0x34, 0x14, 0x27,
		0x3A, 0xCD, 0xB1, 0xAC, 0x89, 0x19, 0x25, 0xF1, 0x86, 0x68, 0xA0, 0xEE, 0x12, 0xBC, 0xA2, 0x75, 0xC1, 0x9D, 0x96, 0x61, 0x74, 0xC3, 0x98,
		0x46, 0x4B, 0x6F, 0x8F, 0x82, 0x9E, 0xCA, 0xAA, 0x2A, 0xF6, 0x1C, 0xE8, 0xB7, 0x8C, 0x58, 0xD4, 0x9F, 0x9C, 0x23, 0x67, 0xBF, 0xD8, 0xC5,
		0x73, 0x86, 0xC2, 0x99, 0xF5, 0x9D, 0xF4, 0x0A, 0xD2, 0x67, 0xE6, 0x75, 0x12, 0xB4, 0xE6, 0x39, 0x55, 0xF7, 0xB4, 0xAD, 0xC4, 0x1D, 0x6D,
		0x43, 0xD4, 0x65, 0xCC, 0x97, 0xE6, 0xEC, 0x30, 0x4A, 0x5E, 0x20, 0x1E, 0xAD, 0x9C, 0xDD, 0xEF, 0x9B, 0xC4, 0xFB, 0xE6, 0xFC, 0x22, 0x77,
		0xC5, 0xD8, 0xC4, 0xB0, 0xC0, 0x16, 0x87, 0xEC, 0x7D, 0xBE, 0xB9, 0x44, 0xD6, 0x7A, 0x95, 0x55, 0xFE, 0x5C, 0xCA, 0x64, 0x4B, 0xD2, 0x7D,
		0x9B, 0xFF, 0xA0, 0xE5, 0xCD, 0x7E, 0x03, 0x9F, 0xA5, 0xE4, 0xB9};
	EXPECT_EQ(xml::CompressUtil::compress(small), smallExpected);

	struct Vector {
		const char* name;
		std::vector<uint8_t> input;
		uint32_t inputCrc;
		size_t outputSize;
		uint32_t outputCrc;
	};
	std::vector<Vector> vectors;
	// 600000 bytes: many dynamic blocks (16383 symbols each), window slides, matches of 258
	vectors.push_back({"text", oracleTextInput(4805, 300000), 0xEF39B9D7u, 44734, 0xAD87C1ACu});
	vectors.push_back({"random 3000", oracleRandomInput(99, 3000), 0xF1815A32u, 3011, 0xB2117CB6u}); // a stored block
	vectors.push_back({"random 70000", oracleRandomInput(123, 70000), 0xFFEA4F2Cu, 70031, 0x3CFEFD07u}); // stored blocks, block start < 0
	vectors.push_back({"fibonacci", oracleFibonacciInput(2024), 0xAB9FF235u, 123059, 0xE1EEF16Cu}); // bit length overflow
	for (const Vector& vector : vectors) {
		SCOPED_TRACE(vector.name);
		ASSERT_EQ(crc32Of(vector.input), vector.inputCrc) << "the generator differs from the Python oracle";
		std::vector<uint8_t> compressed = xml::CompressUtil::compress(vector.input);
		EXPECT_EQ(compressed.size(), vector.outputSize);
		EXPECT_EQ(crc32Of(compressed), vector.outputCrc);
		EXPECT_EQ(xml::CompressUtil::decompress(compressed), vector.input);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------- captcha

TEST(DDSConverterTest, Dxt1Texture) {
	EXPECT_FALSE(captcha::DDSConverter::convertToDxt1NoTransparency(nullptr).has_value());
	captcha::DDSConverter::Image image;
	image.width = 8;
	image.height = 4;
	image.argb.assign(32, static_cast<int32_t>(0xFF000000u)); // left tile black
	for (int y = 0; y < 4; ++y) {
		for (int x = 4; x < 8; ++x)
			image.argb[static_cast<size_t>(y * 8 + x)] = (x + y) % 2 == 0 ? static_cast<int32_t>(0xFFFFFFFFu) : static_cast<int32_t>(0xFF000000u);
	}
	std::optional<commons::utils::ByteBuffer> texture = captcha::DDSConverter::convertToDxt1NoTransparency(&image);
	ASSERT_TRUE(texture.has_value());
	std::span<const uint8_t> bytes = texture->span();
	ASSERT_EQ(bytes.size(), 128u + 16u);
	EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + 4), "DDS ");
	EXPECT_EQ(bytes[4], 124);
	EXPECT_EQ(bytes[12], 4);                                          // height
	EXPECT_EQ(bytes[16], 8);                                          // width
	EXPECT_EQ(bytes[20], 16);                                         // linear size width * height / 2
	EXPECT_EQ(std::string(bytes.begin() + 84, bytes.begin() + 88), "DXT1");
	for (size_t i = 128; i < 136; ++i)
		EXPECT_EQ(bytes[i], 0) << i; // black tile: equal extremes, bitmask 0
	// checkerboard tile: extremes white (0xFFFF) and black, white pixels map to index 0 and black ones to index 1
	EXPECT_EQ(bytes[136], 0xFF);
	EXPECT_EQ(bytes[137], 0xFF);
	EXPECT_EQ(bytes[138], 0x00);
	EXPECT_EQ(bytes[139], 0x00);
	// pixel i (row by row) of the tile is white when (x + y) is even: bits 2i = 0 for white, 1 for black
	uint32_t expectedMask = 0;
	for (uint32_t i = 0; i < 16; ++i) {
		uint32_t x = i % 4 + 4;
		uint32_t y = i / 4;
		if ((x + y) % 2 != 0)
			expectedMask |= 1u << (i * 2);
	}
	uint32_t mask = static_cast<uint32_t>(bytes[140]) | static_cast<uint32_t>(bytes[141]) << 8 | static_cast<uint32_t>(bytes[142]) << 16 |
		static_cast<uint32_t>(bytes[143]) << 24;
	EXPECT_EQ(mask, expectedMask);
}

TEST(CAPTCHAUtilTest, RandomWords) {
	commons::utils::Rnd::seedCurrentThreadForTests(1);
	std::string word = captcha::CAPTCHAUtil::getRandomWord();
	ASSERT_EQ(word.size(), 6u);
	for (char c : word)
		EXPECT_NE(std::string_view("ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789").find(c), std::string_view::npos);
	EXPECT_THROW(captcha::CAPTCHAUtil::createCAPTCHA(word), runtime::UnportedException); // java.awt text rendering
}

// ------------------------------------------------------------------------------------------------------------------------ XmlUtil, HTMLCache

class TempDirectory {
public:
	TempDirectory() : path(std::filesystem::temp_directory_path() / ("aion_gs_base_test_" + std::to_string(std::random_device()()))) {
		std::filesystem::create_directories(path);
	}
	~TempDirectory() {
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
	void write(const std::filesystem::path& relative, std::string_view content) const {
		std::filesystem::create_directories((path / relative).parent_path());
		std::ofstream out(path / relative, std::ios::binary);
		out << content;
	}
	const std::filesystem::path path;
};

TEST(XmlUtilTest, ListFiles) {
	TempDirectory dir;
	dir.write("a.xml", "<a/>");
	dir.write("B.XML", "<b/>");
	dir.write("c.txt", "c");
	dir.write("sub/d.xml", "<d/>");
	std::vector<std::filesystem::path> flat = xml::XmlUtil::listFiles(dir.path, false);
	EXPECT_EQ(flat.size(), 2u);
	std::vector<std::filesystem::path> recursive = xml::XmlUtil::listFiles(dir.path, true);
	EXPECT_EQ(recursive.size(), 3u);
	std::vector<std::filesystem::path> single = xml::XmlUtil::listFiles(dir.path / "a.xml", true);
	EXPECT_EQ(single, std::vector<std::filesystem::path>{dir.path / "a.xml"}) << "Files.find visits the start file";
	// Java: RuntimeException wrapping NoSuchFileException
	try {
		xml::XmlUtil::listFiles(dir.path / "missing", true);
		FAIL() << "expected an exception";
	} catch (const commons::utils::IOException&) {
		FAIL() << "Java wraps the IOException in a RuntimeException";
	} catch (const commons::utils::Exception& e) {
		EXPECT_TRUE(std::string(e.what()).starts_with("java.nio.file.NoSuchFileException: ")) << e.what();
		ASSERT_TRUE(e.cause());
		EXPECT_THROW(std::rethrow_exception(e.cause()), commons::utils::IOException);
	}
}

TEST(XmlUtilTest, ListFilesWalksJunctions) {
	// Java's Windows file attributes treat a junction (a mount point reparse point) as a directory, so FileTreeWalker enters it
	TempDirectory dir;
	dir.write("real/a.xml", "<a/>");
	std::filesystem::create_directories(dir.path / "root");
	std::filesystem::path junction = dir.path / "root" / "junction";
	std::string command = "cmd /c mklink /J \"" + junction.string() + "\" \"" + (dir.path / "real").string() + "\" > NUL 2>&1";
	if (std::system(command.c_str()) != 0 || !std::filesystem::exists(junction))
		GTEST_SKIP() << "no junction could be created";
	EXPECT_EQ(xml::XmlUtil::listFiles(dir.path / "root", true), std::vector<std::filesystem::path>{junction / "a.xml"});
	EXPECT_TRUE(xml::XmlUtil::listFiles(dir.path / "root", false).empty()) << "maxDepth 1 does not open the junction";
	EXPECT_EQ(xml::XmlUtil::listFiles(junction, false), std::vector<std::filesystem::path>{junction / "a.xml"});
	std::filesystem::remove(junction); // removes the junction, not the target's files
}

TEST(XmlUtilTest, ListFilesDoesNotFollowLinks) {
	TempDirectory dir;
	dir.write("real/a.xml", "<a/>");
	std::error_code error;
	std::filesystem::create_symlink(dir.path / "real" / "a.xml", dir.path / "real" / "link.xml", error);
	if (error)
		GTEST_SKIP() << "symbolic links cannot be created here: " << error.message();
	// Files.find without FOLLOW_LINKS: attrs.isRegularFile() is false for the link itself
	EXPECT_EQ(xml::XmlUtil::listFiles(dir.path / "real", false), std::vector<std::filesystem::path>{dir.path / "real" / "a.xml"});
	EXPECT_TRUE(xml::XmlUtil::listFiles(dir.path / "real" / "link.xml", false).empty());
	std::filesystem::create_directory_symlink(dir.path / "real", dir.path / "linkdir", error);
	if (!error) {
		EXPECT_TRUE(xml::XmlUtil::listFiles(dir.path / "linkdir", true).empty()) << "a linked start directory is not walked";
		EXPECT_EQ(xml::XmlUtil::listFiles(dir.path, true).size(), 1u) << "linked subdirectories are not walked";
	}
}

TEST(HTMLCacheTest, LoadsCompactsAndCaches) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	TempDirectory dir;
	dir.write("html/a.xhtml", "<html>\n  <body> Hi </body>\n</html>");
	dir.write("html/sub/b.xhtml", " x ");
	dir.write("html/c.htm", "ignored");
	std::u8string root = (dir.path / "html").u8string();
	std::u8string cacheFile = (dir.path / "html.cache").u8string();
	configs::main::HTMLConfig::HTML_ROOT.set(std::string(root.begin(), root.end()));
	configs::main::HTMLConfig::HTML_CACHE_FILE.set(std::string(cacheFile.begin(), cacheFile.end()));

	cache::HTMLCache& htmlCache = cache::HTMLCache::getInstance();
	EXPECT_EQ(htmlCache.getHTML("a.xhtml"), "<html><body>Hi</body></html>");
	EXPECT_EQ(htmlCache.getHTML("sub/b.xhtml"), "x");
	EXPECT_EQ(htmlCache.getHTML("c.htm"), std::nullopt);
	EXPECT_TRUE(htmlCache.pathExists("sub/b.xhtml"));
	EXPECT_EQ(htmlCache.toString(), "Cache[HTML]: 0.028 kilobytes on 2 file(s) loaded."); // (28 + 1) / 1024f = 0.028320312
	EXPECT_TRUE(std::filesystem::exists(dir.path / "html.cache"));

	htmlCache.reload(false); // from the cache file: no compaction, same content
	EXPECT_EQ(htmlCache.getHTML("a.xhtml"), "<html><body>Hi</body></html>");
	EXPECT_EQ(htmlCache.toString(), "Cache[HTML]: 0.028 kilobytes on 2 file(s) loaded.");

	dir.write("html.cache", "java serialized map"); // not a C++ cache file: rebuilt from the html files
	htmlCache.reload(false);
	EXPECT_EQ(htmlCache.getHTML("sub/b.xhtml"), "x");

	dir.write("html/sub/b.xhtml", "changed");
	EXPECT_EQ(htmlCache.loadFile(dir.path / "html" / "sub" / "b.xhtml"), "changed");
	EXPECT_EQ(htmlCache.getHTML("sub/b.xhtml"), "changed"); // loadFile does not compact
	EXPECT_EQ(htmlCache.loadFile(dir.path / "html" / "c.htm"), std::nullopt);
	EXPECT_EQ(cache::HTMLCache::getRelativePath(dir.path / "html", dir.path / "html" / "sub" / "b.xhtml"), "sub/b.xhtml");
}

} // namespace
} // namespace aion::gameserver::utils
