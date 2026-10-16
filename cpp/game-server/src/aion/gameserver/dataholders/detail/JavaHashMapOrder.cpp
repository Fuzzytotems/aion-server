#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::dataholders::detail {

namespace {

runtime::AtomicInteger treeifiedBuckets{AION_LOCK_CLASS(JavaHashMapOrder::treeifiedBuckets)};

} // namespace

void noteJavaTreeifiedBucket() noexcept {
	if (treeifiedBuckets.getAndIncrement() == 0) {
		try {
			static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.StaticData");
			log.warn("A Java HashMap of the static data would turn a bucket into a tree: the C++ iteration order of that map may differ from Java's");
		} catch (...) { // noexcept: logging must not fail the load
		}
	}
}

int32_t javaTreeifiedBucketCount() noexcept {
	return treeifiedBuckets.get();
}

int32_t javaHashCode(std::string_view utf8) {
	uint32_t h = 0;
	for (char16_t unit : commons::utils::StringUtils::toUtf16(utf8))
		h = 31 * h + unit;
	return static_cast<int32_t>(h);
}

} // namespace aion::gameserver::dataholders::detail
