#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/gameserver/configs/Config.h"

#include "ConfigTestSupport.h"

/**
 * Cross-checks the C++ config classes against the Java sources (game-server/src/com/aionemu/gameserver/configs), independently of how the bind
 * functions were written: the Java annotations are scanned at test time.
 */

using namespace aion::gameserver::configs;
using namespace aion::gameserver::configs::test;
using aion::commons::configuration::ConfigurableProcessor;
using aion::commons::configuration::Properties;
namespace fs = std::filesystem;

namespace {

fs::path javaConfigsDir() {
	return javaGameServerDir() / "src" / "com" / "aionemu" / "gameserver" / "configs";
}

/** @return the simple class names of Java's Config.CONFIGS, in order */
std::vector<std::string> javaConfigClassOrder() {
	std::ifstream in(javaConfigsDir() / "Config.java", std::ios::binary);
	std::stringstream buffer;
	buffer << in.rdbuf();
	const std::string source = buffer.str();
	std::smatch list;
	EXPECT_TRUE(std::regex_search(source, list, std::regex(R"(CONFIGS\s*=\s*Arrays\.asList\(([^;]*)\);)")));
	std::vector<std::string> names;
	const std::string body = list[1].str();
	static const std::regex classLiteral(R"((\w+)\.class)");
	for (auto it = std::sregex_iterator(body.begin(), body.end(), classLiteral); it != std::sregex_iterator(); ++it)
		names.push_back((*it)[1].str());
	return names;
}

/** @return the annotations of all 33 Java config classes named in Config.CONFIGS (without the commons classes) */
std::vector<JavaProperty> scanAllJavaConfigClasses() {
	std::vector<JavaProperty> properties;
	for (const auto& entry : fs::recursive_directory_iterator(javaConfigsDir())) {
		const fs::path& file = entry.path();
		if (file.extension() != ".java" || file.parent_path().filename() == "schedule" || file.parent_path().filename() == "ingameshop" ||
		    file.filename() == "Config.java")
			continue;
		for (JavaProperty& property : scanJavaConfigClass(file))
			properties.push_back(std::move(property));
	}
	return properties;
}

/** a property key matching each @Properties pattern, with a valid value */
const std::map<std::string, std::pair<std::string, std::string>> PATTERN_SAMPLES{
  {"ACCESS_LEVELS", {"somecommand", "7"}},
  {"AUCTION_AUTO_FILL_LIMITS", {"gameserver.housing.auction.auto_fill.limit.HOUSE", "3"}},
  {"TOP_RANKING_QUOTA", {"gameserver.topranking.quota.GENERAL", "30"}},
  {"TOP_RANKING_GP_LOSS", {"gameserver.topranking.gp_loss.GENERAL", "171"}},
  {"THRESHOLD_MILLIS_BY_PACKET_OPCODE", {"gameserver.network.pff.packet.0x10", "100"}},
};

} // namespace

TEST(JavaAnnotationsTest, ConfigClassesInJavaOrder) {
	std::vector<std::string> javaOrder = javaConfigClassOrder();
	ASSERT_EQ(javaOrder.size(), 35u);
	std::vector<std::string> cppOrder;
	for (const Config::ConfigClass& config : Config::getClasses())
		cppOrder.emplace_back(config.simpleName);
	ASSERT_EQ(cppOrder.back(), "RuntimeConfig"); // C++ only
	cppOrder.pop_back();
	EXPECT_EQ(cppOrder, javaOrder);
}

TEST(JavaAnnotationsTest, AnnotationCounts) {
	std::vector<JavaProperty> properties = scanAllJavaConfigClasses();
	std::set<std::string> classes;
	size_t patterns = 0;
	for (const JavaProperty& property : properties) {
		classes.insert(property.className);
		patterns += property.keyPattern.has_value();
	}
	EXPECT_EQ(classes.size(), 33u);
	EXPECT_EQ(properties.size() - patterns, 420u);
	EXPECT_EQ(patterns, 5u);
}

/**
 * Every Java key is bound by a C++ config class (a missing or misspelled key stays unused), every Java default value parses as the C++ field
 * type, and every @Properties pattern is bound.
 */
TEST(JavaAnnotationsTest, EveryJavaKeyAndDefaultValueIsBound) {
	Properties properties;
	std::set<std::string> keys;
	for (const JavaProperty& property : scanAllJavaConfigClasses()) {
		if (property.keyPattern) {
			auto sample = PATTERN_SAMPLES.find(property.fieldName);
			ASSERT_NE(sample, PATTERN_SAMPLES.end()) << property.className << "." << property.fieldName;
			EXPECT_TRUE(std::regex_search(sample->second.first, std::regex(*property.keyPattern))) << *property.keyPattern;
			properties.setProperty(sample->second.first, sample->second.second);
			continue;
		}
		EXPECT_TRUE(keys.insert(property.key).second) << "duplicate key " << property.key;
		// fields without a default value are nullable in Java, where an empty value is valid (null, empty collection, system time zone)
		properties.setProperty(property.key, property.defaultValue.value_or(""));
	}
	ASSERT_EQ(keys.size(), 420u);

	std::vector<ConfigurableProcessor::Binder> binders;
	for (const Config::ConfigClass& config : Config::getClasses())
		binders.emplace_back(config.bind);
	std::set<std::string> unused;
	ASSERT_NO_THROW(unused = ConfigurableProcessor::process(properties, binders));
	EXPECT_TRUE(unused.empty()) << "not bound: " << testing::PrintToString(unused);
}

TEST(JavaAnnotationsTest, ScannerDecodesJavaLiterals) {
	EXPECT_EQ(decodeJavaStringLiteral(R"(»GM«%s, \"x\")"), "»GM«%s, \"x\"");
	EXPECT_EQ(decodeJavaStringLiteral(R"(^gameserver\\.topranking\\.quota\\.(.+))"), R"(^gameserver\.topranking\.quota\.(.+))");
}
