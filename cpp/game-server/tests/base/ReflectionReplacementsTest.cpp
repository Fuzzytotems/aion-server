// P4-05 replacements of Java reflection (handlers-and-porting-plan.md §1.10): JavaColor (java.awt.Color constants and getRGB), simpleClassName
// (Class.getSimpleName) and enumValueOf (Enum.valueOf with Java's "No enum constant" message).

#include <gtest/gtest.h>

#include <string>
#include <typeinfo>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/utils/time/gametime/GameTime_Month.h"

namespace aion::gameserver::utils {
namespace simple_class_name_test {
struct Outer {
	struct Inner {};
};
template <class T>
struct Generic {};
} // namespace simple_class_name_test

namespace {

TEST(JavaColorTest, ConstantsHaveAwtArgbValues) {
	EXPECT_EQ(JavaColor::RED.getRGB(), static_cast<int32_t>(0xFFFF0000u));
	EXPECT_EQ(JavaColor::RED.getRGB(), -65536);
	EXPECT_EQ(JavaColor::WHITE.getRGB(), -1);
	EXPECT_EQ(JavaColor::BLACK.getRGB(), static_cast<int32_t>(0xFF000000u));
	EXPECT_EQ(JavaColor::PINK.getRGB(), static_cast<int32_t>(0xFFFFAFAFu));
	EXPECT_EQ(JavaColor::ORANGE.getRGB(), static_cast<int32_t>(0xFFFFC800u));
	EXPECT_EQ(JavaColor::LIGHT_GRAY.getRGB(), static_cast<int32_t>(0xFFC0C0C0u));
	EXPECT_EQ(JavaColor::DARK_GRAY.getRed(), 64);
	EXPECT_EQ(JavaColor::MAGENTA.getGreen(), 0);
	EXPECT_EQ(JavaColor::CYAN.getBlue(), 255);
	EXPECT_EQ(JavaColor::GRAY.getAlpha(), 255);
}

TEST(JavaColorTest, ByNameFindsOnlyUpperCaseFields) {
	EXPECT_EQ(JavaColor::byName("BLUE"), JavaColor::BLUE);
	EXPECT_EQ(JavaColor::byName("LIGHT_GRAY"), JavaColor::LIGHT_GRAY);
	EXPECT_EQ(JavaColor::byName("blue"), std::nullopt); // Dye upper-cases the parameter first
	EXPECT_EQ(JavaColor::byName("PURPLE"), std::nullopt);
	EXPECT_EQ(JavaColor::byName("lightGray"), std::nullopt);
}

TEST(JavaColorTest, ConstructorsFollowAwt) {
	JavaColor color(1, 2, 3);
	EXPECT_EQ(color.getRGB(), static_cast<int32_t>(0xFF010203u));
	EXPECT_EQ(JavaColor(10, 20, 30, 40).getAlpha(), 40);
	EXPECT_EQ(JavaColor(0x12345678).getRGB(), static_cast<int32_t>(0xFF345678u)); // Color(int rgb): alpha forced to 255
	EXPECT_TRUE(JavaColor(255, 0, 0).equals(JavaColor::RED));
	EXPECT_EQ(JavaColor::RED.hashCode(), -65536);
	try {
		JavaColor(256, -1, 3, 300);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Color parameter outside of expected range: Alpha Red Green");
	}
}

TEST(SimpleClassNameTest, StripsNamespacesOuterClassesAndTemplateArguments) {
	EXPECT_EQ(simpleClassName(typeid(model::geometry::PolyArea)), "PolyArea");
	EXPECT_EQ(simpleClassName(typeid(JavaColor)), "JavaColor");
	EXPECT_EQ(simpleClassName(typeid(simple_class_name_test::Outer::Inner)), "Inner");
	EXPECT_EQ(simpleClassName(typeid(simple_class_name_test::Generic<model::geometry::PolyArea>)), "Generic");
	EXPECT_EQ(simpleClassName(typeid(runtime::IllegalArgumentException)), "IllegalArgumentException");
}

TEST(EnumValueOfTest, FindsConstantsByExactName) {
	EXPECT_EQ(enumValueOf<model::Race>("ASMODIANS"), model::Race::ASMODIANS);
	EXPECT_EQ(enumValueOf<model::siege::SiegeRace>("BALAUR"), model::siege::SiegeRace::BALAUR);
}

TEST(EnumValueOfTest, UnknownNameThrowsJavaMessage) {
	try {
		enumValueOf<model::siege::SiegeRace>("invalidName");
		FAIL() << "expected EnumConstantException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "No enum constant com.aionemu.gameserver.model.siege.SiegeRace.invalidName");
		const auto* enumException = dynamic_cast<const EnumConstantException*>(&e);
		ASSERT_NE(enumException, nullptr);
		EXPECT_EQ(enumException->getEnumSimpleName(), "SiegeRace");
		EXPECT_EQ(enumException->getEnumCanonicalName(), "com.aionemu.gameserver.model.siege.SiegeRace");
		EXPECT_EQ(enumException->getValue(), "invalidName");
		EXPECT_EQ(enumException->getAllValues(), (std::vector<std::string>{"ELYOS", "ASMODIANS", "BALAUR"}));
	}
	try {
		enumValueOf<model::Race>("elyos"); // case-sensitive like Java
		FAIL() << "expected EnumConstantException";
	} catch (const EnumConstantException& e) {
		EXPECT_STREQ(e.what(), "No enum constant com.aionemu.gameserver.model.Race.elyos");
	}
}

TEST(EnumValueOfTest, NestedEnumUsesJavaCanonicalName) {
	try {
		enumValueOf<time::gametime::GameTime_Month>("SMARCH");
		FAIL() << "expected EnumConstantException";
	} catch (const EnumConstantException& e) {
		EXPECT_STREQ(e.what(), "No enum constant com.aionemu.gameserver.utils.time.gametime.GameTime.Month.SMARCH");
		EXPECT_EQ(e.getEnumSimpleName(), "Month");
		EXPECT_EQ(e.getAllValues().size(), 12u);
	}
}

} // namespace
} // namespace aion::gameserver::utils
