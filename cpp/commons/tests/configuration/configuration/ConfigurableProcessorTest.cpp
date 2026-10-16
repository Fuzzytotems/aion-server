#include "aion/commons/configuration/ConfigurableProcessor.h"

#include <unordered_map>

#include <gtest/gtest.h>

using namespace aion::commons;
using namespace aion::commons::configuration;

namespace {

enum class AbyssRankEnum { GRADE9_SOLDIER, STAR1_OFFICER, STAR2_OFFICER, GENERAL };

Properties props(std::initializer_list<std::pair<const char*, const char*>> entries, std::shared_ptr<const Properties> defaults = nullptr) {
	Properties p(std::move(defaults));
	for (const auto& [key, value] : entries)
		p.setProperty(key, value);
	return p;
}

/** @return the messages of the exception and its causes, outermost first */
std::vector<std::string> messageChain(const std::exception& e) {
	std::vector<std::string> messages{e.what()};
	if (auto* ex = dynamic_cast<const utils::Exception*>(&e); ex && ex->cause()) {
		try {
			std::rethrow_exception(ex->cause());
		} catch (const std::exception& cause) {
			std::vector<std::string> causes = messageChain(cause);
			messages.insert(messages.end(), causes.begin(), causes.end());
		}
	}
	return messages;
}

} // namespace

TEST(ConfigurableProcessorTest, DefaultValueConstant) {
	EXPECT_EQ(Property::DEFAULT_VALUE, "DO_NOT_OVERWRITE_INITIALIAZION_VALUE");
}

TEST(ConfigurableProcessorTest, BindWithAndWithoutDefault) {
	Properties p = props({{"present", "42"}});
	ConfigurableProcessor processor(p);

	int32_t present = 1, missingWithDefault = 1, missingWithoutDefault = 7;
	std::string text = "initial";
	processor.bind("present", present, "5");
	processor.bind("missing.default", missingWithDefault, "5");
	processor.bind("missing.nodefault", missingWithoutDefault);
	processor.bind("missing.text", text, "");
	EXPECT_EQ(present, 42);
	EXPECT_EQ(missingWithDefault, 5);
	EXPECT_EQ(missingWithoutDefault, 7); // keeps its initial value
	EXPECT_EQ(text, "");
	EXPECT_TRUE(processor.unusedProperties().empty());
}

TEST(ConfigurableProcessorTest, ValueEqualToDefaultValueConstantLeavesFieldUnmodified) {
	Properties p = props({{"a", "DO_NOT_OVERWRITE_INITIALIAZION_VALUE"}, {"ref", "DO_NOT_OVERWRITE_INITIALIAZION_VALUE"}, {"b", "${ref}"}});
	ConfigurableProcessor processor(p);
	int32_t a = 3, b = 4;
	processor.bind("a", a, "1");
	processor.bind("b", b, "1");
	EXPECT_EQ(a, 3);
	EXPECT_EQ(b, 4);
	EXPECT_EQ(processor.unusedProperties(), (std::set<std::string>{"ref"}));
}

TEST(ConfigurableProcessorTest, LookupThroughDefaultsAndUnusedTracking) {
	auto defaults = std::make_shared<Properties>(props({{"from.defaults", "1"}, {"overridden", "2"}, {"unused.default", "x"}}));
	Properties p = props({{"overridden", "3"}, {"same.as.default", "5"}, {"unused.own", "y"}}, defaults);

	int32_t fromDefaults = 0, overridden = 0, sameAsDefault = 0, missing = 0;
	std::set<std::string> unused = ConfigurableProcessor::process(p, {[&](ConfigurableProcessor& processor) {
		                                                              processor.bind("from.defaults", fromDefaults, "9");
		                                                              processor.bind("overridden", overridden, "9");
		                                                              processor.bind("same.as.default", sameAsDefault,
		                                                                             "5"); // value equals the default, but the key exists: it is used
		                                                              processor.bind("missing", missing, "9");
	                                                              }});
	EXPECT_EQ(fromDefaults, 1);
	EXPECT_EQ(overridden, 3);
	EXPECT_EQ(sameAsDefault, 5);
	EXPECT_EQ(missing, 9);
	EXPECT_EQ(unused, (std::set<std::string>{"unused.default", "unused.own"}));
}

TEST(ConfigurableProcessorTest, ProcessRunsAllBindersWithOneUnusedSet) {
	Properties p = props({{"a", "1"}, {"b", "2"}, {"c", "3"}});
	int32_t a = 0, b = 0;
	std::vector<ConfigurableProcessor::Binder> binders{[&](ConfigurableProcessor& processor) { processor.bind("a", a); },
	                                                   [&](ConfigurableProcessor& processor) { processor.bind("b", b); }};
	EXPECT_EQ(ConfigurableProcessor::process(p, binders), (std::set<std::string>{"c"}));
	EXPECT_EQ(a, 1);
	EXPECT_EQ(b, 2);
	EXPECT_EQ(ConfigurableProcessor::process(p, std::span<const ConfigurableProcessor::Binder>()), (std::set<std::string>{"a", "b", "c"}));
}

TEST(ConfigurableProcessorTest, QuotedEmptyString) {
	Properties p = props({{"quoted", "\"\""}, {"spaced", " \t\"\"  "}, {"notEmpty", "\"\"x"}, {"csv", "\"\""}, {"number", "\"\""}});
	ConfigurableProcessor processor(p);
	std::string quoted = "x", spaced = "x", notEmpty, defaultQuoted = "x";
	std::vector<std::string> csv{"x"};
	processor.bind("quoted", quoted);
	processor.bind("spaced", spaced);
	processor.bind("notEmpty", notEmpty);
	processor.bind("missing", defaultQuoted, "\"\"");
	processor.bind("csv", csv);
	EXPECT_EQ(quoted, "");
	EXPECT_EQ(spaced, "");
	EXPECT_EQ(notEmpty, "\"\"x");
	EXPECT_EQ(defaultQuoted, "");
	EXPECT_TRUE(csv.empty());
	int32_t number = 0;
	try {
		processor.bind("number", number);
		FAIL();
	} catch (const TransformationException& e) {
		EXPECT_EQ(messageChain(e),
		          (std::vector<std::string>{"Error modifying field for property number", "Error parsing \"\" as int", "Zero length string"}));
	}
}

TEST(ConfigurableProcessorTest, PlaceholderReplacement) {
	auto defaults = std::make_shared<Properties>(props({{"dir", "./log"}}));
	Properties p = props({{"name", "server"},
	                      {"file", "${dir}/${name}.log"},
	                      {"missing", "a${nope}b"},
	                      {"repeated", "${name}-${name}"},
	                      {"notRecursive", "${file}"},
	                      {"quirk", "${indirect}${name}"},
	                      {"indirect", "${name}"},
	                      {"unterminated", "${name"},
	                      {"empty", "${}${name}"},
	                      {"nested", "${${name}}"},
	                      {"dollar", "$${name}"}},
	                     defaults);
	ConfigurableProcessor processor(p);
	auto value = [&](std::string_view key) {
		std::string result;
		processor.bind(key, result);
		return result;
	};
	EXPECT_EQ(value("file"), "./log/server.log");
	EXPECT_EQ(value("missing"), "ab");
	EXPECT_EQ(value("repeated"), "server-server");
	EXPECT_EQ(value("notRecursive"), "${dir}/${name}.log");
	// Java replaces all occurrences of each token found in the original value, including those introduced by earlier replacements
	EXPECT_EQ(value("quirk"), "serverserver");
	EXPECT_EQ(value("unterminated"), "${name");
	EXPECT_EQ(value("empty"), "${}server");
	EXPECT_EQ(value("nested"), "}"); // token is "${name", which is not a property
	EXPECT_EQ(value("dollar"), "$server");

	std::string fromDefault;
	processor.bind("not.there", fromDefault, "${name}.default");
	EXPECT_EQ(fromDefault, "server.default");
	// placeholders do not mark the referenced properties as used
	EXPECT_EQ(processor.unusedProperties(), (std::set<std::string>{"dir", "indirect", "name"}));
}

TEST(ConfigurableProcessorTest, TransformationErrorsNameKeyAndValue) {
	Properties p = props({{"ok", "1"}, {"bad", "12abc"}});
	ConfigurableProcessor processor(p);
	int32_t ok = 0, bad = 5;
	processor.bind("ok", ok);
	try {
		processor.bind("bad", bad, "3");
		FAIL();
	} catch (const TransformationException& e) {
		EXPECT_EQ(messageChain(e),
		          (std::vector<std::string>{"Error modifying field for property bad", "Error parsing \"12abc\" as int", "For input string: \"12abc\""}));
	}
	EXPECT_EQ(ok, 1);
	EXPECT_EQ(bad, 5); // unchanged on error

	std::vector<int32_t> list{1};
	try {
		processor.bind("missing", list, "1, 2, x");
		FAIL();
	} catch (const TransformationException& e) {
		EXPECT_EQ(messageChain(e), (std::vector<std::string>{"Error modifying field for property missing", "Error parsing \"1, 2, x\" as int[]",
		                                                     "Error parsing \"x\" as int", "For input string: \"x\""}));
	}
	EXPECT_EQ(list, std::vector<int32_t>{1});
}

TEST(ConfigurableProcessorTest, BindPatternWithGroup) {
	auto defaults =
	  std::make_shared<Properties>(props({{"gameserver.topranking.quota.STAR1_OFFICER", "1000"}, {"gameserver.topranking.quota.GENERAL", "30"}}));
	Properties p =
	  props({{"gameserver.topranking.quota.GENERAL", "${general}"}, {"general", "31"}, {"gameserver.topranking.legion_limit", "50"}}, defaults);
	ConfigurableProcessor processor(p);
	std::map<AbyssRankEnum, int32_t> quota{{AbyssRankEnum::GRADE9_SOLDIER, 1}};
	processor.bindPattern("^gameserver\\.topranking\\.quota\\.(.+)", quota);
	EXPECT_EQ(quota, (std::map<AbyssRankEnum, int32_t>{{AbyssRankEnum::STAR1_OFFICER, 1000}, {AbyssRankEnum::GENERAL, 31}})); // replaced entirely
	EXPECT_EQ(processor.unusedProperties(), (std::set<std::string>{"gameserver.topranking.legion_limit", "general"}));
}

TEST(ConfigurableProcessorTest, BindPatternWithoutGroupUsesWholeKeyAndFindSemantics) {
	Properties p = props({{"access", "9"}, {"add", "8"}, {"gameserver.x", "1"}, {"with space", "1"}});
	ConfigurableProcessor processor(p);
	std::map<std::string, int8_t> accessLevels;
	processor.bindPattern("^[a-zA-Z0-9_]+$", accessLevels);
	EXPECT_EQ(accessLevels, (std::map<std::string, int8_t>{{"access", 9}, {"add", 8}}));

	std::unordered_map<std::string, std::string> containing;
	processor.bindPattern("x", containing); // partial match like Matcher.find
	EXPECT_EQ(containing, (std::unordered_map<std::string, std::string>{{"gameserver.x", "1"}}));
	EXPECT_EQ(processor.unusedProperties(), (std::set<std::string>{"with space"}));

	std::map<std::string, std::string> none{{"old", "entry"}};
	processor.bindPattern("^nothing$", none);
	EXPECT_TRUE(none.empty());
}

TEST(ConfigurableProcessorTest, BindPatternHexKeys) {
	Properties p = props({{"gameserver.network.pff.mode", "1"},
	                      {"gameserver.network.pff.packet.0x10", "500"},
	                      {"gameserver.network.pff.packet.0X1a", "\"\""},
	                      {"gameserver.network.pff.packet.0x", "1"}});
	ConfigurableProcessor processor(p);
	std::map<int32_t, std::string> thresholds;
	processor.bindPattern("^gameserver\\.network\\.pff\\.packet\\.(0[xX][0-9a-fA-F]+)$", thresholds);
	EXPECT_EQ(thresholds, (std::map<int32_t, std::string>{{0x10, "500"}, {0x1a, ""}}));
}

TEST(ConfigurableProcessorTest, BindPatternOptionalGroupThatDidNotParticipate) {
	Properties p = props({{"b", "1"}});
	ConfigurableProcessor processor(p);
	std::map<std::string, int32_t> map;
	processor.bindPattern("(a)?b", map);
	EXPECT_EQ(map, (std::map<std::string, int32_t>{{"", 1}}));
}

TEST(ConfigurableProcessorTest, BindPatternErrors) {
	Properties p = props({{"rank.CAPTAIN", "1"}, {"level.GENERAL", "x"}});
	ConfigurableProcessor processor(p);
	std::map<AbyssRankEnum, int32_t> map;
	try {
		processor.bindPattern("^rank\\.(.+)", map);
		FAIL();
	} catch (const TransformationException& e) {
		EXPECT_EQ(messageChain(e), (std::vector<std::string>{"Error modifying field for properties matching ^rank\\.(.+)",
		                                                     "Error parsing \"CAPTAIN\" as AbyssRankEnum", "No enum constant AbyssRankEnum.CAPTAIN"}));
	}
	try {
		processor.bindPattern("^level\\.(.+)", map);
		FAIL();
	} catch (const TransformationException& e) {
		EXPECT_EQ(messageChain(e),
		          (std::vector<std::string>{"Error modifying field for properties matching ^level\\.(.+)", "Could not transform property: GENERAL",
		                                    "Error parsing \"x\" as int", "For input string: \"x\""}));
	}
	try {
		processor.bindPattern("([unclosed", map);
		FAIL();
	} catch (const TransformationException& e) {
		EXPECT_THROW(std::rethrow_exception(e.cause()), std::regex_error);
	}
}

TEST(ConfigurableProcessorTest, StaticTransformForRuntimeChanges) {
	EXPECT_EQ(ConfigurableProcessor::transform<std::vector<float>>("1, 2.5"), (std::vector<float>{1.0f, 2.5f}));
	std::vector<std::pair<std::string, std::string>> entries{{"STAR2_OFFICER", "7"}};
	auto map = ConfigurableProcessor::transformMap<std::map<AbyssRankEnum, int32_t>>(entries);
	EXPECT_EQ(map, (std::map<AbyssRankEnum, int32_t>{{AbyssRankEnum::STAR2_OFFICER, 7}}));
	try {
		ConfigurableProcessor::transform<bool>("yes");
		FAIL();
	} catch (const TransformationException& e) {
		// the //configure admin command shows e.getCause().getMessage()
		EXPECT_EQ(messageChain(e)[1], "Only true, false, 1 and 0 are allowed.");
	}
}
