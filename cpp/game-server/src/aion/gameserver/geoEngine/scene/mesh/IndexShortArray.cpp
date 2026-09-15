#include "aion/gameserver/geoEngine/scene/mesh/IndexShortArray.h"

#include <cstddef>
#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/utils/TempVars.h"

namespace aion::gameserver::geoEngine::scene::mesh {

IndexShortArray::IndexShortArray(runtime::Ref<runtime::Array<int16_t>> bufValue) : buf(std::move(bufValue)) {
}

IndexShortArray::~IndexShortArray() = default;

runtime::Ref<IndexShortArray> IndexShortArray::create(runtime::Ref<runtime::Array<int16_t>> bufValue) {
	return runtime::makeRef<IndexShortArray>(std::move(bufValue));
}

runtime::Ref<IndexShortArray> IndexShortArray::create(std::span<const int16_t> bufValue) {
	runtime::Ref<runtime::Array<int16_t>> array = runtime::Array<int16_t>::make(static_cast<int32_t>(bufValue.size()));
	runtime::Array<int16_t>& elements = *array; // one checked dereference, not one per element (performance)
	for (size_t i = 0; i < bufValue.size(); ++i)
		elements[static_cast<int32_t>(i)].set(bufValue[i]);
	return create(std::move(array));
}

int32_t IndexShortArray::get(int32_t i) {
	return buf->get(i) & 0xFFFF;
}

int32_t IndexShortArray::size() {
	return buf->length();
}

void IndexShortArray::swap(int32_t i1, int32_t i2) {
	int32_t p1 = i1 * 3;
	int32_t p2 = i2 * 3;
	utils::TempVars& vars = utils::TempVars::get();
	utils::TempVars::ReleaseGuard releaseOnException(vars);
	runtime::Array<int16_t>& array = *buf;
	// Java System.arraycopy checks both ranges before copying
	if (p1 < 0 || p2 < 0 || p1 > array.length() - 3 || p2 > array.length() - 3)
		throw runtime::ArrayIndexOutOfBoundsException("arraycopy: last source index " + std::to_string(std::max(p1, p2) + 3) + " out of bounds for short[" +
			std::to_string(array.length()) + "]");
	// store p1 in tmp
	for (int32_t k = 0; k < 3; ++k)
		vars.bihSwapTmpShort[static_cast<size_t>(k)] = array.get(p1 + k);

	// copy p2 to p1
	for (int32_t k = 0; k < 3; ++k)
		array[p1 + k].set(array.get(p2 + k));

	// copy tmp to p2
	for (int32_t k = 0; k < 3; ++k)
		array[p2 + k].set(vars.bihSwapTmpShort[static_cast<size_t>(k)]);
	vars.release();
}

int32_t IndexShortArray::hashCode() const {
	// Java record hashCode: the identity hash of the array (not reproducible; any value consistent with identity equality)
	return static_cast<int32_t>(std::hash<const runtime::Array<int16_t>*>()(buf.get()));
}

} // namespace aion::gameserver::geoEngine::scene::mesh
