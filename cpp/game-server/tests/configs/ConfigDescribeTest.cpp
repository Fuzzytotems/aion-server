#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/Properties.h"
#include "aion/commons/configuration/TransformationException.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/configs/detail/ConfigEnums.h"

/**
 * C2 cp4 (lane A, 2026-10-18): ConfigurableProcessor::describe and the named binds of the config classes (//configure). The golden file
 * game-server/tests/configs/config_binds.golden.txt was dumped from the bind functions' source (tools/porting/config_bind_dump.py) before the
 * binds were named, so equal rows prove that naming them changed no key, default or order.
 */

using namespace aion::gameserver::configs;
using aion::commons::configuration::ConfigurableProcessor;
using aion::commons::configuration::ConfigValue;
using aion::commons::configuration::Properties;
using aion::commons::configuration::TransformationException;

namespace {

std::string escape(std::string text) {
	std::string out;
	for (char c : text) {
		if (c == '\\')
			out += "\\\\";
		else if (c == '\t')
			out += "\\t";
		else if (c == '\n')
			out += "\\n";
		else
			out += c;
	}
	return out;
}

std::vector<std::string> goldenRows() {
	std::ifstream in(std::filesystem::path(__FILE__).parent_path() / "config_binds.golden.txt", std::ios::binary);
	std::vector<std::string> rows;
	for (std::string line; std::getline(in, line);) {
		std::erase(line, '\r');
		if (!line.empty())
			rows.push_back(line);
	}
	return rows;
}

} // namespace

/** every bind of every config class, by describe(), equals the source dump taken before the binds were named */
TEST(ConfigDescribeTest, DescribeListsEveryBindOfEveryConfigClassAsTheGoldenDump) {
	std::vector<Config::ConfigClass> classes(Config::getClasses().begin(), Config::getClasses().end());
	std::ranges::stable_sort(classes, {}, &Config::ConfigClass::simpleName);
	std::vector<std::string> rows;
	int unnamed = 0;
	for (const Config::ConfigClass& cls : classes) {
		for (const ConfigurableProcessor::FieldInfo& field : ConfigurableProcessor::describe(cls.bind)) {
			std::string fieldName = field.fieldName;
			if (fieldName.empty()) { // a bind of a local: CommonsConfig's script compiler flag only
				EXPECT_EQ(cls.simpleName, "CommonsConfig");
				EXPECT_FALSE(field.toJavaString);
				EXPECT_FALSE(field.set);
				fieldName = "scriptCompilerCaching";
				unnamed++;
			} else {
				EXPECT_TRUE(field.toJavaString) << cls.simpleName << "." << fieldName;
			}
			rows.push_back(std::string(cls.simpleName) + "\t" + (field.isPattern ? "pattern" : "bind") + "\t" + fieldName + "\t" + escape(field.key) +
				"\t" + (field.defaultValue ? escape(*field.defaultValue) : std::string("-")));
		}
	}
	EXPECT_EQ(unnamed, 1);
	const std::vector<std::string> golden = goldenRows();
	ASSERT_EQ(golden.size(), 449u); // C++-dev: + RuntimeConfig.DIFFICULTY_MENU_ENABLE
	EXPECT_EQ(rows, golden);
}

/** set and setMap exist for exactly the fields that may change at runtime: every one but the six plain DatabaseConfig fields */
TEST(ConfigDescribeTest, OnlyThePlainDatabaseFieldsCannotBeSet) {
	std::vector<std::string> refused;
	for (const Config::ConfigClass& cls : Config::getClasses()) {
		for (const ConfigurableProcessor::FieldInfo& field : ConfigurableProcessor::describe(cls.bind)) {
			if (field.fieldName.empty())
				continue;
			EXPECT_FALSE(field.isPattern ? static_cast<bool>(field.set) : static_cast<bool>(field.setMap)) << field.fieldName;
			if (field.isPattern ? !field.setMap : !field.set)
				refused.push_back(std::string(cls.simpleName) + "." + field.fieldName);
		}
	}
	for (const ConfigurableProcessor::FieldInfo& field : ConfigurableProcessor::describe(&aion::commons::configs::DatabaseConfig::bind)) {
		if (field.fieldName != "DATABASE_SOCKET_TIMEOUT")
			continue;
		const std::optional<int32_t> saved = aion::commons::configs::DatabaseConfig::DATABASE_SOCKET_TIMEOUT;
		aion::commons::configs::DatabaseConfig::DATABASE_SOCKET_TIMEOUT.reset();
		EXPECT_EQ(field.toJavaString(), "null"); // commons' formatter (BasicJavaString)
		aion::commons::configs::DatabaseConfig::DATABASE_SOCKET_TIMEOUT = 30;
		EXPECT_EQ(field.toJavaString(), "30");
		aion::commons::configs::DatabaseConfig::DATABASE_SOCKET_TIMEOUT = saved;
	}
	std::ranges::sort(refused);
	EXPECT_EQ(refused, (std::vector<std::string>{"DatabaseConfig.DATABASE_CONNECTIONS_MAX", "DatabaseConfig.DATABASE_PASSWORD",
						   "DatabaseConfig.DATABASE_SOCKET_TIMEOUT", "DatabaseConfig.DATABASE_TIMEOUT", "DatabaseConfig.DATABASE_URL",
						   "DatabaseConfig.DATABASE_USER"}));
}

namespace {

struct Sample {
	static inline std::atomic<bool> FLAG{true};
	static inline std::atomic<int32_t> NUMBER{-7};
	static inline std::atomic<float> RATE{1.0f};
	static inline std::atomic<double> FACTOR{0.1};
	static inline std::atomic<detail::AbyssRankEnum> RANK{detail::AbyssRankEnum::GRADE9_SOLDIER};
	static inline ConfigValue<std::string> NAME{std::string("abc")};
	static inline ConfigValue<std::optional<int32_t>> MAYBE;
	static inline ConfigValue<std::vector<int32_t>> LIST{std::vector<int32_t>{3, 1, 2}};
	static inline ConfigValue<std::unordered_set<int32_t>> HASH_SET{std::unordered_set<int32_t>{30, 10, 20}};
	static inline ConfigValue<std::regex> PATTERN;
	static inline ConfigValue<std::optional<std::wregex>> MAYBE_PATTERN;
	static inline ConfigValue<std::unordered_map<int32_t, int32_t>> BY_ID;
	static inline std::string PLAIN = "plain";

	static void bind(ConfigurableProcessor& p) {
		AION_BIND(p, "s.flag", FLAG);
		AION_BIND(p, "s.number", NUMBER);
		AION_BIND(p, "s.rate", RATE);
		AION_BIND(p, "s.factor", FACTOR);
		AION_BIND(p, "s.rank", RANK);
		AION_BIND(p, "s.name", NAME, "abc");
		AION_BIND(p, "s.maybe", MAYBE);
		AION_BIND(p, "s.list", LIST);
		AION_BIND(p, "s.hash_set", HASH_SET);
		AION_BIND(p, "s.pattern", PATTERN, "[a-z]{2,4}");
		AION_BIND(p, "s.maybe_pattern", MAYBE_PATTERN);
		AION_BIND_PATTERN(p, "^s\\.by_id\\.(\\d+)$", BY_ID);
		AION_BIND(p, "s.plain", PLAIN);
	}
};

std::map<std::string, ConfigurableProcessor::FieldInfo> sampleFields() {
	std::map<std::string, ConfigurableProcessor::FieldInfo> fields;
	for (ConfigurableProcessor::FieldInfo& field : ConfigurableProcessor::describe(&Sample::bind))
		fields.emplace(field.fieldName, std::move(field));
	return fields;
}

} // namespace

/** toJavaString prints the values as Java's String.valueOf: floats as Float.toString, enums by name, hash sets sorted (C++), maps {k=v} */
TEST(ConfigDescribeTest, ToJavaStringPrintsEachTypeFamilyAsJava) {
	Properties properties;
	properties.setProperty("s.by_id.7", "70");
	properties.setProperty("s.by_id.3", "30");
	ConfigurableProcessor::process(properties, {&Sample::bind});
	std::map<std::string, ConfigurableProcessor::FieldInfo> fields = sampleFields();
	EXPECT_EQ(fields.size(), 13u);
	EXPECT_EQ(fields.at("FLAG").toJavaString(), "true");
	EXPECT_EQ(fields.at("NUMBER").toJavaString(), "-7");
	EXPECT_EQ(fields.at("RATE").toJavaString(), "1.0");
	EXPECT_EQ(fields.at("FACTOR").toJavaString(), "0.1");
	EXPECT_EQ(fields.at("RANK").toJavaString(), "GRADE9_SOLDIER");
	EXPECT_EQ(fields.at("NAME").toJavaString(), "abc");
	EXPECT_EQ(fields.at("MAYBE").toJavaString(), "null");
	EXPECT_EQ(fields.at("LIST").toJavaString(), "[3, 1, 2]");
	EXPECT_EQ(fields.at("HASH_SET").toJavaString(), "[10, 20, 30]");
	EXPECT_EQ(fields.at("PATTERN").toJavaString(), "[a-z]{2,4}"); // the remembered source of the default
	EXPECT_EQ(fields.at("MAYBE_PATTERN").toJavaString(), "null");
	EXPECT_EQ(fields.at("BY_ID").toJavaString(), "{3=30, 7=70}");
	EXPECT_EQ(fields.at("PLAIN").toJavaString(), "plain");
	EXPECT_EQ(fields.at("PATTERN").defaultValue, std::optional<std::string>("[a-z]{2,4}"));
	EXPECT_TRUE(fields.at("BY_ID").isPattern);
	EXPECT_FALSE(fields.at("NAME").isPattern);
}

/** set transforms and stores (atomic, ConfigValue; a regex remembers its new source), throws TransformationException, is empty for a plain field */
TEST(ConfigDescribeTest, SetStoresThroughTheAtomicOrTheConfigValueOnly) {
	std::map<std::string, ConfigurableProcessor::FieldInfo> fields = sampleFields();
	fields.at("NUMBER").set("12");
	EXPECT_EQ(Sample::NUMBER.load(), 12);
	fields.at("RANK").set("GRADE8_SOLDIER");
	EXPECT_EQ(Sample::RANK.load(), detail::AbyssRankEnum::GRADE8_SOLDIER);
	fields.at("NAME").set("x y");
	EXPECT_EQ(*Sample::NAME.get(), "x y");
	fields.at("PATTERN").set("b+");
	EXPECT_EQ(fields.at("PATTERN").toJavaString(), "b+");
	EXPECT_TRUE(std::regex_match("bbb", *Sample::PATTERN.get()));
	fields.at("MAYBE_PATTERN").set("c?");
	EXPECT_EQ(fields.at("MAYBE_PATTERN").toJavaString(), "c?");
	fields.at("BY_ID").setMap({{"5", "50"}});
	EXPECT_EQ(fields.at("BY_ID").toJavaString(), "{5=50}");
	EXPECT_THROW(fields.at("NUMBER").set("twelve"), TransformationException);
	EXPECT_EQ(Sample::NUMBER.load(), 12);
	EXPECT_FALSE(fields.at("PLAIN").set);
	EXPECT_FALSE(fields.at("BY_ID").set);
	EXPECT_FALSE(fields.at("NAME").setMap);
}

/** describe() reads no property and changes no field */
TEST(ConfigDescribeTest, DescribeBindsNothing) {
	Sample::NUMBER = 99;
	Sample::PLAIN = "kept";
	std::vector<ConfigurableProcessor::FieldInfo> fields = ConfigurableProcessor::describe(&Sample::bind);
	EXPECT_EQ(fields.size(), 13u);
	EXPECT_EQ(Sample::NUMBER.load(), 99);
	EXPECT_EQ(Sample::PLAIN, "kept");
	EXPECT_EQ(fields[0].fieldName, "FLAG");
	EXPECT_EQ(fields[0].key, "s.flag");
	EXPECT_EQ(fields[0].defaultValue, std::nullopt);
}
