// Adapters (SpaceSeparatedBytesAdapter, LocalDateTimeAdapter) with Java semantics, and HolderRef publication.

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "aion/gameserver/dataholders/loadingutils/HolderRef.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/LocalDateTimeAdapter.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/SpaceSeparatedBytesAdapter.h"

#include "XmlTestModel.h"

namespace aion::gameserver::xml::test {
namespace {

using adapters::LocalDateTime;
using adapters::parseLocalDateTime;
using adapters::parseSpaceSeparatedBytes;
using adapters::printLocalDateTime;
using adapters::printSpaceSeparatedBytes;

TEST(SpaceSeparatedBytesAdapterTest, FollowsStringSplitAndByteParseByte) {
	EXPECT_EQ(parseSpaceSeparatedBytes("1 2 3"), (std::vector<int8_t>{1, 2, 3}));
	EXPECT_EQ(parseSpaceSeparatedBytes("-128 +127"), (std::vector<int8_t>{-128, 127}));
	EXPECT_EQ(parseSpaceSeparatedBytes("1 2 "), (std::vector<int8_t>{1, 2})) << "trailing empty strings are removed by split";
	EXPECT_EQ(parseSpaceSeparatedBytes("   "), (std::vector<int8_t>{})) << "only empty parts: split returns an empty array";
	for (std::string_view bad : {"", " 1", "1  2", "128", "-129", "1\t2", "a", "+"})
		EXPECT_THROW(parseSpaceSeparatedBytes(bad), XmlValueException) << "'" << bad << "'";
	EXPECT_EQ(printSpaceSeparatedBytes(std::vector<int8_t>{1, -2, 3}), "1 -2 3");
	EXPECT_EQ(printSpaceSeparatedBytes({}), "");
}

TEST(LocalDateTimeAdapterTest, ParsesIsoLocalDateTime) {
	using namespace std::chrono;
	EXPECT_EQ(parseLocalDateTime("2026-08-03T00:00:00"), local_days{2026y / August / 3d});
	EXPECT_EQ(parseLocalDateTime("2015-02-06T13:45"), local_days{2015y / February / 6d} + 13h + 45min);
	EXPECT_EQ(parseLocalDateTime("2016-02-29t23:59:59.5"), local_days{2016y / February / 29d} + 23h + 59min + 59s + 500ms);
	EXPECT_EQ(parseLocalDateTime("2016-02-29T23:59:59.123000000"), local_days{2016y / February / 29d} + 23h + 59min + 59s + 123ms);
	EXPECT_EQ(parseLocalDateTime("2000-01-01T00:00:00."), local_days{2000y / January / 1d});
	for (std::string_view bad :
	     {"", "2015-02-06", "2015-02-06 13:45", "2015-2-06T13:45", "2015-02-06T13", "2015-02-06T24:00", "2015-02-06T23:60", "2015-02-06T23:59:60",
	      "2015-13-01T00:00", "2015-02-29T00:00", "2015-04-31T00:00", "2015-00-10T00:00", "2015-01-00T00:00", " 2015-02-06T13:45", "2015-02-06T13:45Z",
	      "+2015-02-06T13:45", "12015-02-06T13:45", "2015-02-06T13:45:00.0001", "2015-02-06T13:45:00.1234567890"})
		EXPECT_THROW(parseLocalDateTime(bad), XmlValueException) << bad;
}

TEST(LocalDateTimeAdapterTest, PrintsLikeLocalDateTimeToString) {
	using namespace std::chrono;
	EXPECT_EQ(printLocalDateTime(local_days{2026y / August / 3d}), "2026-08-03T00:00");
	EXPECT_EQ(printLocalDateTime(local_days{2026y / August / 3d} + 5s), "2026-08-03T00:00:05");
	EXPECT_EQ(printLocalDateTime(local_days{2026y / August / 3d} + 7ms), "2026-08-03T00:00:00.007");
	for (std::string_view text : {"1999-12-31T23:59:59.999", "2015-02-06T13:45", "2016-02-29T00:00:01"})
		EXPECT_EQ(printLocalDateTime(parseLocalDateTime(text)), text);
}

TEST(HolderRefTest, PublishesOnceAndStaysValid) {
	HolderRef<ItemData> ref;
	EXPECT_FALSE(ref);
	EXPECT_EQ(ref.get(), nullptr);
	EXPECT_THROW(static_cast<void>(ref->size()), runtime::NullPointerException);
	EXPECT_THROW(ref.publish(nullptr), runtime::IllegalArgumentException);

	auto holder = std::make_unique<ItemData>();
	const ItemData* raw = holder.get();
	ref.publish(std::move(holder));
	EXPECT_TRUE(ref);
	EXPECT_EQ(ref.get(), raw);
	EXPECT_EQ(&*ref, raw);
	EXPECT_EQ(ref->size(), 0u);
	EXPECT_THROW(ref.publish(std::make_unique<ItemData>()), runtime::IllegalStateException);
	EXPECT_EQ(ref.get(), raw);
	ref.resetForTests();
	EXPECT_EQ(ref.get(), nullptr);
	delete raw; // the test owns what it published

	MutableHolderRef<Stat> mutableRef;
	EXPECT_THROW(static_cast<void>(mutableRef->value), runtime::NullPointerException);
	auto stat = std::make_unique<Stat>();
	Stat* statRaw = stat.get();
	mutableRef.publish(std::move(stat));
	mutableRef->value = 5;
	EXPECT_EQ(statRaw->value, 5);
	EXPECT_THROW(mutableRef.publish(std::make_unique<Stat>()), runtime::IllegalStateException);
	mutableRef.resetForTests();
	delete statRaw;
}

TEST(HolderRefTest, ReadersRacingWithPublishSeeNullOrTheHolder) {
	HolderRef<Stat> ref;
	auto holder = std::make_unique<Stat>();
	holder->value = 42;
	const Stat* raw = holder.get();
	std::atomic<bool> start{false};
	std::atomic<int32_t> bad{0};
	std::vector<std::thread> readers;
	for (int i = 0; i < 4; ++i) {
		readers.emplace_back([&] {
			while (!start.load())
				std::this_thread::yield();
			for (int n = 0; n < 20000; ++n) {
				const Stat* seen = ref.get();
				if (seen != nullptr && (seen != raw || seen->value != 42))
					bad.fetch_add(1);
			}
		});
	}
	start = true;
	ref.publish(std::move(holder));
	for (auto& reader : readers)
		reader.join();
	EXPECT_EQ(bad.load(), 0);
	ref.resetForTests();
	delete raw;
}

} // namespace
} // namespace aion::gameserver::xml::test
