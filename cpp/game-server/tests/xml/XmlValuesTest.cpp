// Value converters (JAXB DatatypeConverterImpl semantics, docs/design/static-data.md §2.4), EnumTraits tables, nameHash and ChildCounts.

#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "XmlTestModel.h"

namespace aion::gameserver::xml::test {
namespace {

TEST(XmlValuesTest, IntegersTrimXmlWhitespaceAndAcceptOneSign) {
	EXPECT_EQ(parseInt32("42"), 42);
	EXPECT_EQ(parseInt32(" \t\r\n42\n "), 42);
	EXPECT_EQ(parseInt32("+42"), 42);
	EXPECT_EQ(parseInt32("-42"), -42);
	EXPECT_EQ(parseInt32("007"), 7);
	EXPECT_EQ(parseInt32("-0"), 0);
	for (std::string_view bad : {"", " ", "+", "-", "4 2", "--1", "+-1", "1.0", "0x10", "12a",
	                             "\xC2\xA0"
	                             "1"})
		EXPECT_THROW(parseInt32(bad), XmlValueException) << bad;
}

TEST(XmlValuesTest, IntegerRangesAreCheckedPerType) {
	EXPECT_EQ(parseInt8("127"), 127);
	EXPECT_EQ(parseInt8("-128"), -128);
	EXPECT_THROW(parseInt8("128"), XmlValueException);
	EXPECT_THROW(parseInt8("-129"), XmlValueException);
	EXPECT_EQ(parseInt16("-32768"), -32768);
	EXPECT_THROW(parseInt16("32768"), XmlValueException);
	EXPECT_EQ(parseInt32("2147483647"), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(parseInt32("-2147483648"), std::numeric_limits<int32_t>::min());
	EXPECT_THROW(parseInt32("2147483648"), XmlValueException);
	EXPECT_EQ(parseInt64("9223372036854775807"), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(parseInt64("-9223372036854775808"), std::numeric_limits<int64_t>::min());
	EXPECT_THROW(parseInt64("9223372036854775808"), XmlValueException);
	EXPECT_THROW(parseInt64("99999999999999999999999"), XmlValueException);
}

TEST(XmlValuesTest, FloatsFollowParseFloatWithoutJavaOnlyForms) {
	EXPECT_EQ(parseFloat("1.5"), 1.5f);
	EXPECT_EQ(parseFloat(" 0.1 "), 0.1f) << "correctly rounded like Float.parseFloat";
	EXPECT_EQ(parseFloat("+2"), 2.0f);
	EXPECT_EQ(parseFloat("-.5"), -0.5f);
	EXPECT_EQ(parseFloat("3."), 3.0f);
	EXPECT_EQ(parseFloat("1e3"), 1000.0f);
	EXPECT_EQ(parseFloat("1.5E-2"), 0.015f);
	EXPECT_TRUE(std::isnan(parseFloat("NaN")));
	EXPECT_EQ(parseFloat("INF"), std::numeric_limits<float>::infinity());
	EXPECT_EQ(parseFloat("-INF"), -std::numeric_limits<float>::infinity());
	EXPECT_TRUE(std::signbit(parseFloat("-0.0")));
	EXPECT_EQ(parseDouble("0.1"), 0.1);
	EXPECT_EQ(parseDouble("1e308"), 1e308);
	for (std::string_view bad : {"", ".", "1f", "1d", "0x1p3", "inf", "nan", "Infinity", "1e", "e5", "1.2.3", "1,5", "- 1", "++1"})
		EXPECT_THROW(parseFloat(bad), XmlValueException) << bad;
	EXPECT_THROW(parseFloat("1e39"), XmlValueException) << "overflow is an error (Java: Infinity)";
	EXPECT_THROW(parseDouble("1e999"), XmlValueException);
}

TEST(XmlValuesTest, BooleansAcceptTrueFalseOneZero) {
	EXPECT_TRUE(parseBool("true"));
	EXPECT_TRUE(parseBool(" 1 "));
	EXPECT_FALSE(parseBool("false"));
	EXPECT_FALSE(parseBool("0"));
	for (std::string_view bad : {"", "TRUE", "True", "yes", "2", "t"})
		EXPECT_THROW(parseBool(bad), XmlValueException) << bad;
}

TEST(XmlValuesTest, EnumsUseXmlValuesAndTrim) {
	EXPECT_EQ(parseEnum<Race>("ELYOS"), Race::ELYOS);
	EXPECT_EQ(parseEnum<Race>(" PC_ALL\n"), Race::PC_ALL);
	EXPECT_EQ(parseEnum<ZoneAttribute>("glide"), ZoneAttribute::GLIDE) << "@XmlEnumValue";
	EXPECT_THROW(parseEnum<ZoneAttribute>("GLIDE"), XmlValueException) << "the constant name is not the XML value";
	try {
		parseEnum<Race>("elyos");
		FAIL();
	} catch (const XmlValueException& e) {
		EXPECT_STREQ(e.what(), "Unknown Race constant 'elyos'");
	}
}

TEST(XmlValuesTest, EnumTraitsLookups) {
	EXPECT_EQ(enumName(Race::ASMODIANS), "ASMODIANS");
	EXPECT_EQ(enumName(ZoneAttribute::RECALL), "RECALL");
	EXPECT_EQ(enumOrdinal(ZoneAttribute::GLIDE), 2);
	EXPECT_EQ(enumFromName<ZoneAttribute>("RECALL"), ZoneAttribute::RECALL);
	EXPECT_EQ(enumFromName<ZoneAttribute>("recall"), std::nullopt);
	EXPECT_EQ(enumFromXml<ZoneAttribute>("recall"), ZoneAttribute::RECALL);
	EXPECT_EQ(enumFromXml<Race>(""), std::nullopt);
	EXPECT_EQ(enumName(static_cast<Race>(9)), "");

	struct Entry {
		std::string_view name;
	};
	static constexpr Entry unsorted[] = {{"b"}, {"a"}};
	static constexpr Entry duplicate[] = {{"a"}, {"a"}};
	static constexpr Entry sorted[] = {{"A"}, {"_"}, {"a"}};
	EXPECT_FALSE(isStrictlySortedByName(unsorted));
	EXPECT_FALSE(isStrictlySortedByName(duplicate));
	EXPECT_TRUE(isStrictlySortedByName(sorted)) << "byte order: 'A' < '_' < 'a'";
}

TEST(XmlValuesTest, StringsAreUnchanged) {
	EXPECT_EQ(parseValue<std::string>("  a  b "), "  a  b ");
	EXPECT_EQ(parseValue<std::string>(""), "");
}

TEST(XmlValuesTest, XmlListsSplitOnWhitespaceRuns) {
	EXPECT_EQ(splitXmlList(" 1  2\t3\n"), (std::vector<std::string_view>{"1", "2", "3"}));
	EXPECT_TRUE(splitXmlList("").empty());
	EXPECT_TRUE(splitXmlList(" \n ").empty());
	EXPECT_EQ(parseList<std::vector<int32_t>>("5 -6"), (std::vector<int32_t>{5, -6}));
	EXPECT_EQ(parseList<std::optional<std::vector<Race>>>(""), std::optional<std::vector<Race>>(std::vector<Race>{})) << "present-empty";
	EXPECT_EQ(parseList<std::unordered_set<Race>>("ELYOS ELYOS ASMODIANS"), (std::unordered_set<Race>{Race::ELYOS, Race::ASMODIANS}));
	EXPECT_THROW(parseList<std::vector<int8_t>>("1 1000"), XmlValueException);
}

TEST(XmlValuesTest, NameHashIsFnv1a) {
	EXPECT_EQ(nameHash(""), 0xcbf29ce484222325ull);
	EXPECT_EQ(nameHash("a"), 0xaf63dc4c8601ec8cull);
	EXPECT_EQ("item_template"_xh, nameHash("item_template"));
	EXPECT_NE("mask"_xh, "Mask"_xh);
}

TEST(XmlValuesTest, ChildCountsCountElementChildrenByName) {
	auto document = XmlDocument::parseString("<r><a/>text<b/><a/><!-- c --><a><a/></a><c/></r>");
	ChildCounts counts;
	counts.addChildren(document->root());
	EXPECT_EQ(counts["a"], 3u) << "grandchildren are not counted";
	EXPECT_EQ(counts["b"], 1u);
	EXPECT_EQ(counts["c"], 1u);
	EXPECT_EQ(counts["x"], 0u);
	EXPECT_EQ(counts.total(), 5u);
	counts.addChildren(document->root());
	EXPECT_EQ(counts["a"], 6u) << "counts accumulate over several roots";
}

} // namespace
} // namespace aion::gameserver::xml::test
