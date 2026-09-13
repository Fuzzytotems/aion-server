#include "aion/commons/configuration/ConfigValue.h"

#include <atomic>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/configuration/ConfigurableProcessor.h"

using namespace aion::commons;
using namespace aion::commons::configuration;

namespace {

enum class Race { ELYOS, ASMODIANS };

Properties props(std::initializer_list<std::pair<const char*, const char*>> entries) {
	Properties p;
	for (const auto& [key, value] : entries)
		p.setProperty(key, value);
	return p;
}

struct ReloadedConfig {
	static inline ConfigValue<std::vector<std::string>> WORDS;
	static inline ConfigValue<std::map<std::string, int32_t>> LEVELS;
	static inline std::atomic<int32_t> RATE{0};

	static void bind(ConfigurableProcessor& p) {
		p.bind("words", WORDS, "");
		p.bindPattern("^level\\.(.+)$", LEVELS);
		p.bind("rate", RATE, "0");
	}
};

} // namespace

TEST(ConfigValueTest, InitialValueAndSnapshots) {
	ConfigValue<std::string> empty;
	ASSERT_NE(empty.get(), nullptr);
	EXPECT_EQ(*empty.get(), "");

	ConfigValue<std::vector<std::string>> words(std::vector<std::string>{"a", "b"});
	std::shared_ptr<const std::vector<std::string>> snapshot = words.get();
	words.set({"c"});
	EXPECT_EQ(*snapshot, (std::vector<std::string>{"a", "b"})); // an old snapshot stays valid and unchanged
	EXPECT_EQ(*words.get(), std::vector<std::string>{"c"});
}

TEST(ConfigValueTest, BindWithAndWithoutDefault) {
	Properties p = props({{"words", "x, y"}, {"pattern", "[a-z]+"}, {"bad", "12abc"}, {"level", "7"}, {"flag", "true"}, {"race", "ASMODIANS"}});
	ConfigurableProcessor processor(p);

	ConfigValue<std::vector<std::string>> words;
	ConfigValue<std::optional<std::regex>> pattern;
	ConfigValue<int32_t> missingWithDefault(1), missingWithoutDefault(7);
	processor.bind("words", words);
	processor.bind("pattern", pattern, "");
	processor.bind("missing.default", missingWithDefault, "5");
	processor.bind("missing.nodefault", missingWithoutDefault);
	EXPECT_EQ(*words.get(), (std::vector<std::string>{"x", "y"}));
	ASSERT_TRUE(pattern.get()->has_value());
	EXPECT_TRUE(std::regex_match("abc", **pattern.get()));
	EXPECT_EQ(*missingWithDefault.get(), 5);
	EXPECT_EQ(*missingWithoutDefault.get(), 7); // keeps its initial value

	ConfigValue<int32_t> bad(3);
	auto before = bad.get();
	EXPECT_THROW(processor.bind("bad", bad, "1"), TransformationException);
	EXPECT_EQ(bad.get(), before); // no new snapshot on error

	std::atomic<int8_t> level{0};
	std::atomic<bool> flag{false}, missingFlag{true};
	std::atomic<Race> race{Race::ELYOS};
	processor.bind("level", level, "1");
	processor.bind("flag", flag);
	processor.bind("missing.flag", missingFlag);
	processor.bind("race", race, "ELYOS");
	EXPECT_EQ(level.load(), 7);
	EXPECT_TRUE(flag.load());
	EXPECT_TRUE(missingFlag.load());
	EXPECT_EQ(race.load(), Race::ASMODIANS);
	std::atomic<int32_t> badAtomic{4};
	EXPECT_THROW(processor.bind("bad", badAtomic), TransformationException);
	EXPECT_EQ(badAtomic.load(), 4);

	EXPECT_TRUE(processor.unusedProperties().empty());
}

TEST(ConfigValueTest, BindPattern) {
	Properties p = props({{"access.add", "3"}, {"access.kick", "1"}, {"other", "x"}});
	ConfigurableProcessor processor(p);
	ConfigValue<std::map<std::string, int8_t>> accessLevels(std::map<std::string, int8_t>{{"old", 9}});
	processor.bindPattern("^access\\.(.+)$", accessLevels);
	EXPECT_EQ(*accessLevels.get(), (std::map<std::string, int8_t>{{"add", 3}, {"kick", 1}}));
	EXPECT_EQ(processor.unusedProperties(), std::set<std::string>{"other"});

	auto before = accessLevels.get();
	EXPECT_THROW(processor.bindPattern("^(other)$", accessLevels), TransformationException);
	EXPECT_EQ(accessLevels.get(), before);
}

/**
 * The game server reloads all configs while other threads read them (Java: Config.load() on event start/stop and //reload). Readers of
 * ConfigValue and std::atomic fields must always see a complete old or new value. With plain std::vector/std::map fields this test reads freed
 * memory (and typically crashes in a debug build).
 */
TEST(ConfigValueTest, ConcurrentReloadWhileReading) {
	const Properties first = props({{"words", "alpha, beta, gamma"}, {"level.a", "1"}, {"level.b", "2"}, {"rate", "1"}});
	const Properties second = props({{"words", "delta, epsilon"}, {"level.c", "3"}, {"rate", "2"}});
	ConfigurableProcessor::process(first, {&ReloadedConfig::bind});

	std::atomic<bool> stop{false};
	std::atomic<int> invalidReads{0};
	std::vector<std::thread> readers;
	for (int i = 0; i < 4; i++) {
		readers.emplace_back([&] {
			while (!stop.load()) {
				auto words = ReloadedConfig::WORDS.get();
				std::string joined;
				for (const std::string& word : *words)
					joined += word + ';';
				auto levels = ReloadedConfig::LEVELS.get();
				int32_t sum = 0;
				for (const auto& [name, level] : *levels)
					sum += level;
				int32_t rate = ReloadedConfig::RATE.load();
				if ((joined != "alpha;beta;gamma;" && joined != "delta;epsilon;") || (sum != 3) || (rate != 1 && rate != 2))
					invalidReads++;
			}
		});
	}
	std::vector<std::thread> writers;
	for (int i = 0; i < 2; i++) { // two reloads at the same time, like an event starting during //reload config
		writers.emplace_back([&, i] {
			for (int n = 0; n < 1000; n++)
				ConfigurableProcessor::process((n + i) % 2 == 0 ? first : second, {&ReloadedConfig::bind});
		});
	}
	for (std::thread& writer : writers)
		writer.join();
	stop = true;
	for (std::thread& reader : readers)
		reader.join();
	EXPECT_EQ(invalidReads.load(), 0);
}
