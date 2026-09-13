#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <map>
#include <numeric>
#include <ranges>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"

using namespace aion::commons;
using namespace aion::commons::utils;

namespace {

constexpr int SAMPLES = 200'000;

/** Pearson's chi-squared statistic of uniformly expected bucket counts */
double chiSquared(const std::vector<int>& counts, int total) {
	double expected = static_cast<double>(total) / static_cast<double>(counts.size());
	double sum = 0;
	for (int count : counts)
		sum += (count - expected) * (count - expected) / expected;
	return sum;
}

enum class Color { RED, GREEN, BLUE };

struct Item {
	int id;
	bool operator==(const Item&) const = default;
};

} // namespace

TEST(RndTest, GetIsInclusiveAndUniform) {
	std::vector<int> counts(7);
	for (int i = 0; i < SAMPLES; i++) {
		int32_t value = Rnd::get(3, 9);
		ASSERT_GE(value, 3);
		ASSERT_LE(value, 9);
		counts[value - 3]++;
	}
	// 6 degrees of freedom: p = 0.0001 at 27.9
	EXPECT_LT(chiSquared(counts, SAMPLES), 27.9);
	EXPECT_EQ(Rnd::get(5, 5), 5);
}

TEST(RndTest, GetFullIntRange) {
	bool negative = false;
	bool positive = false;
	for (int i = 0; i < 1000; i++) {
		int32_t value = Rnd::get(INT32_MIN, INT32_MAX);
		negative |= value < 0;
		positive |= value > 0;
	}
	EXPECT_TRUE(negative && positive);
	EXPECT_EQ(Rnd::get(INT32_MAX, INT32_MAX), INT32_MAX);
	int32_t top = Rnd::get(INT32_MAX - 1, INT32_MAX);
	EXPECT_TRUE(top == INT32_MAX - 1 || top == INT32_MAX);
}

TEST(RndTest, GetWithMaxBelowMinWarnsAndReturnsMin) {
	std::ostringstream stream;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
	sink->set_pattern("%l|%v");
	logging::LoggerFactory::configure("com.aionemu.commons.utils.Rnd", {.sinks = {sink}, .additive = false});

	EXPECT_EQ(Rnd::get(10, 2), 10);

	std::string out = stream.str();
	EXPECT_TRUE(out.starts_with("warning|aion::commons::utils::IllegalArgumentException: max < min")) << out;
	EXPECT_NE(out.find("\tat "), std::string::npos); // stack trace
	logging::LoggerFactory::removeConfig("com.aionemu.commons.utils.Rnd");
}

TEST(RndTest, NextIntBounds) {
	std::vector<int> counts(10);
	for (int i = 0; i < SAMPLES; i++) {
		int32_t value = Rnd::nextInt(10);
		ASSERT_GE(value, 0);
		ASSERT_LT(value, 10);
		counts[value]++;
	}
	EXPECT_LT(chiSquared(counts, SAMPLES), 33.7); // 9 dof, p = 0.0001
	for (int i = 0; i < 10000; i++) {
		int32_t value = Rnd::nextInt(-5, 5);
		ASSERT_GE(value, -5);
		ASSERT_LT(value, 5);
	}
	EXPECT_EQ(Rnd::nextInt(1), 0);
	EXPECT_EQ(Rnd::nextInt(7, 8), 7);
	int32_t wide = Rnd::nextInt(INT32_MIN, INT32_MAX);
	EXPECT_LT(wide, INT32_MAX);
}

TEST(RndTest, InvalidBoundsThrowLikeJava) {
	auto expectMessage = [](auto&& call, std::string_view message) {
		try {
			call();
			ADD_FAILURE() << "no exception for " << message;
		} catch (const IllegalArgumentException& e) {
			EXPECT_EQ(std::string_view(e.what()), message);
		}
	};
	expectMessage([] { Rnd::nextInt(0); }, "bound must be positive");
	expectMessage([] { Rnd::nextInt(-3); }, "bound must be positive");
	expectMessage([] { Rnd::nextLong(0); }, "bound must be positive");
	expectMessage([] { Rnd::nextInt(5, 5); }, "bound must be greater than origin");
	expectMessage([] { Rnd::nextLong(6, 5); }, "bound must be greater than origin");
	expectMessage([] { Rnd::nextFloat(0.0f); }, "bound must be finite and positive");
	expectMessage([] { Rnd::nextFloat(std::numeric_limits<float>::infinity()); }, "bound must be finite and positive");
	expectMessage([] { Rnd::nextDouble(std::nan("")); }, "bound must be finite and positive");
	expectMessage([] { Rnd::nextFloat(1.0f, 1.0f); }, "bound must be greater than origin");
	expectMessage([] { Rnd::nextDouble(-std::numeric_limits<double>::infinity(), 0.0); }, "bound must be greater than origin");
	expectMessage([] { Rnd::ints(3, 2); }, "bound must be greater than origin");
}

TEST(RndTest, NextLongFullRange) {
	for (int i = 0; i < 1000; i++) {
		int64_t value = Rnd::nextLong(INT64_MIN, INT64_MAX);
		ASSERT_LT(value, INT64_MAX);
		int64_t big = Rnd::nextLong(INT64_MAX);
		ASSERT_GE(big, 0);
	}
	std::set<int64_t> values;
	for (int i = 0; i < 100; i++)
		values.insert(Rnd::nextLong());
	EXPECT_GT(values.size(), 95u);
}

TEST(RndTest, FloatingPointRanges) {
	double sum = 0;
	for (int i = 0; i < SAMPLES; i++) {
		float f = Rnd::nextFloat();
		ASSERT_GE(f, 0.0f);
		ASSERT_LT(f, 1.0f);
		sum += f;
		float c = Rnd::chance();
		ASSERT_GE(c, 0.0f);
		ASSERT_LT(c, 100.0f);
		float angle = Rnd::nextFloat(360.0f);
		ASSERT_GE(angle, 0.0f);
		ASSERT_LT(angle, 360.0f);
		float ranged = Rnd::nextFloat(1.0f, 2.0f);
		ASSERT_GE(ranged, 1.0f);
		ASSERT_LT(ranged, 2.0f);
		double d = Rnd::nextDouble(-1.0, 1.0);
		ASSERT_GE(d, -1.0);
		ASSERT_LT(d, 1.0);
	}
	EXPECT_NEAR(sum / SAMPLES, 0.5, 0.005);
	// huge ranges must not overflow
	float huge = Rnd::nextFloat(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	EXPECT_TRUE(std::isfinite(huge));
	// tiny ranges are corrected to stay below the bound
	float tiny = Rnd::nextFloat(1.0f, std::nextafter(1.0f, 2.0f));
	EXPECT_EQ(tiny, 1.0f);
}

TEST(RndTest, ChanceSemantics) {
	for (int i = 0; i < 10000; i++) {
		ASSERT_FALSE(Rnd::chance() < 0.0f);   // 0% always fails
		ASSERT_TRUE(Rnd::chance() < 100.0f);  // 100% always succeeds
	}
}

TEST(RndTest, NextBooleanIsBalanced) {
	int trues = 0;
	for (int i = 0; i < SAMPLES; i++)
		trues += Rnd::nextBoolean();
	EXPECT_NEAR(static_cast<double>(trues) / SAMPLES, 0.5, 0.01);
}

TEST(RndTest, NextBytesFillsEveryByte) {
	std::array<uint8_t, 13> bytes{};
	std::array<int, 13> nonZero{};
	for (int i = 0; i < 100; i++) {
		Rnd::nextBytes(bytes);
		for (size_t k = 0; k < bytes.size(); k++)
			nonZero[k] += bytes[k] != 0;
	}
	for (int count : nonZero)
		EXPECT_GT(count, 90);
	Rnd::nextBytes({}); // empty is fine
}

TEST(RndTest, GetElementReturnsPointerOrNull) {
	std::vector<Item> items{{1}, {2}, {3}};
	std::map<int, int> counts;
	for (int i = 0; i < 3000; i++) {
		Item* item = Rnd::get(items);
		ASSERT_NE(item, nullptr);
		ASSERT_GE(item - items.data(), 0);
		ASSERT_LT(item - items.data(), 3);
		counts[item->id]++;
	}
	EXPECT_EQ(counts.size(), 3u);

	const std::vector<Item> constItems{{7}};
	const Item* single = Rnd::get(constItems);
	EXPECT_EQ(single, &constItems[0]);

	std::vector<Item> empty;
	EXPECT_EQ(Rnd::get(empty), nullptr);

	Item array[] = {{4}, {5}};
	Item* fromArray = Rnd::get(array);
	EXPECT_TRUE(fromArray == &array[0] || fromArray == &array[1]);

	std::span<Item> span(items); // borrowed temporary views return pointers too
	Item* fromSpan = Rnd::get(std::span<Item>(items));
	EXPECT_GE(fromSpan - span.data(), 0);

	std::list<std::string> names{"a", "b"}; // non-random-access ranges work too
	std::string* name = Rnd::get(names);
	ASSERT_NE(name, nullptr);
	EXPECT_TRUE(*name == "a" || *name == "b");
}

TEST(RndTest, OptionalForTemporaries) {
	std::optional<std::string> value = Rnd::get(std::vector<std::string>{"x", "y"});
	ASSERT_TRUE(value.has_value());
	EXPECT_TRUE(*value == "x" || *value == "y");
	EXPECT_FALSE(Rnd::get(std::vector<std::string>{}).has_value());

	std::vector<std::unique_ptr<int>> owners;
	owners.push_back(std::make_unique<int>(42));
	std::optional<std::unique_ptr<int>> moved = Rnd::get(std::move(owners));
	ASSERT_TRUE(moved.has_value());
	EXPECT_EQ(**moved, 42);

	// ranges with prvalue references (e.g. transform views) also return the value
	std::vector<int> source{1, 2, 3};
	auto doubled = source | std::views::transform([](int i) { return static_cast<int64_t>(i) * 2; });
	std::optional<int64_t> element = Rnd::get(doubled);
	ASSERT_TRUE(element.has_value());
	EXPECT_EQ(*element % 2, 0);
}

TEST(RndTest, IntRangesReturnValuesLikeJavaIntArrays) {
	static constexpr int32_t NPC_IDS[] = {215074, 215075, 215076};
	std::set<int32_t> seen;
	for (int i = 0; i < 1000; i++)
		seen.insert(Rnd::get(NPC_IDS));
	EXPECT_EQ(seen, (std::set<int32_t>{215074, 215075, 215076}));

	std::vector<int32_t> ids{9};
	EXPECT_EQ(Rnd::get(ids), 9);
	EXPECT_THROW(Rnd::get(std::vector<int32_t>{}), IllegalArgumentException);
	try {
		std::vector<int32_t> none;
		Rnd::get(none);
	} catch (const IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Cannot get random int from an empty array.");
	}

	int32_t literal = Rnd::get({230852, 233316, 233317});
	EXPECT_TRUE(literal == 230852 || literal == 233316 || literal == 233317);
	EXPECT_EQ(Rnd::get({Color::GREEN}), Color::GREEN);
	EXPECT_THROW(Rnd::get(std::initializer_list<int32_t>{}), IllegalArgumentException);
}

TEST(RndTest, GetOptionalLikeJavaListOfIntegers) {
	// Java: HalloweenPumpkinAI returns Rnd.get(itemIds) for a List<Integer> and checks the result for null
	std::vector<int32_t> none;
	EXPECT_EQ(Rnd::getOptional(none), std::nullopt);
	EXPECT_EQ(Rnd::getOptional(std::vector<int32_t>{}), std::nullopt);
	std::vector<int32_t> itemIds{188052, 188053};
	std::optional<int32_t> itemId = Rnd::getOptional(itemIds);
	ASSERT_TRUE(itemId);
	EXPECT_TRUE(*itemId == 188052 || *itemId == 188053);
	EXPECT_EQ(Rnd::getOptional(std::vector<std::string>{"only"}), "only");
}

TEST(RndTest, EnumValuesLikeJavaValuesArray) {
	static constexpr std::array COLORS{Color::RED, Color::GREEN, Color::BLUE};
	std::set<Color> seen;
	for (int i = 0; i < 300; i++)
		seen.insert(*Rnd::get(COLORS));
	EXPECT_EQ(seen.size(), 3u);
}

TEST(RndTest, StreamsAreLazyAndBounded) {
	std::vector<int32_t> values;
	for (int32_t value : Rnd::ints(0, 3) | std::views::take(1000))
		values.push_back(value);
	ASSERT_EQ(values.size(), 1000u);
	EXPECT_TRUE(std::ranges::all_of(values, [](int32_t v) { return v >= 0 && v < 3; }));
	EXPECT_EQ(std::set<int32_t>(values.begin(), values.end()).size(), 3u);

	for (double d : Rnd::doubles(5.0, 6.0) | std::views::take(100)) {
		ASSERT_GE(d, 5.0);
		ASSERT_LT(d, 6.0);
	}
	for (int64_t l : Rnd::longs(-2, 0) | std::views::take(100)) {
		ASSERT_GE(l, -2);
		ASSERT_LT(l, 0);
	}
	auto unbounded = Rnd::ints() | std::views::take(3);
	EXPECT_EQ(std::ranges::distance(unbounded.begin(), unbounded.end()), 3);
	EXPECT_EQ(std::ranges::distance(Rnd::longs() | std::views::take(2)), 2);
	EXPECT_EQ(std::ranges::distance(Rnd::doubles() | std::views::take(2)), 2);
}

TEST(RndTest, ThreadsGetIndependentSequences) {
	std::array<std::vector<uint64_t>, 4> sequences;
	std::vector<std::thread> threads;
	for (auto& sequence : sequences) {
		threads.emplace_back([&sequence] {
			for (int i = 0; i < 16; i++)
				sequence.push_back(Rnd::generator()());
		});
	}
	for (auto& thread : threads)
		thread.join();
	for (size_t a = 0; a < sequences.size(); a++) {
		for (size_t b = a + 1; b < sequences.size(); b++)
			EXPECT_NE(sequences[a], sequences[b]);
	}
}

TEST(RndTest, GeneratorWorksWithStandardAlgorithms) {
	std::vector<int> values(100);
	std::iota(values.begin(), values.end(), 0);
	std::vector<int> shuffled = values;
	std::shuffle(shuffled.begin(), shuffled.end(), Rnd::generator());
	EXPECT_NE(shuffled, values);
	std::ranges::sort(shuffled);
	EXPECT_EQ(shuffled, values);
}

TEST(RndTest, JumpedGeneratorsDiverge) {
	Rnd::Xoshiro256PlusPlus a(0);
	Rnd::Xoshiro256PlusPlus b(0);
	EXPECT_EQ(a(), b());
	b.jump();
	EXPECT_NE(a(), b());
}
