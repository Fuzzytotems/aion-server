#include "aion/gameserver/geoEngine/collision/CollisionResults.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"

namespace aion::gameserver::geoEngine::collision {

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue, runtime::Ptr<IgnoreProperties> ignorePropertiesValue)
	: CollisionResults(intentionsValue, instanceIdValue, false, ignorePropertiesValue) {
}

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue)
	: CollisionResults(intentionsValue, instanceIdValue, false, nullptr) {
}

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue, bool searchFirst)
	: CollisionResults(intentionsValue, instanceIdValue, searchFirst, nullptr) {
}

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue, bool searchFirst,
	runtime::Ptr<IgnoreProperties> ignorePropertiesValue)
	: intentions(intentionsValue), instanceId(instanceIdValue), onlyFirst(searchFirst), ignoreProperties(ignorePropertiesValue) {
}

namespace {

/** Java: ArrayList.get/set index check message */
void checkIndex(int32_t index, size_t size) {
	if (index < 0 || static_cast<size_t>(index) >= size)
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(size));
}

} // namespace

void CollisionResults::clear() {
	results.clear();
}

void CollisionResults::sortIfNeeded() {
	if (!sorted) {
		// Java: results.sort(null) - a stable merge sort by CollisionResult.compareTo
		std::stable_sort(results.begin(), results.end(), [](const CollisionResult& a, const CollisionResult& b) { return a.compareTo(b) < 0; });
		sorted = true;
	}
}

runtime::JavaIterator<CollisionResult> CollisionResults::iterator() {
	sortIfNeeded();
	// Java: ArrayList.iterator(); remove() deletes the last returned element (its snapshot index minus the elements removed before it)
	auto removed = std::make_shared<size_t>(0);
	return runtime::JavaIterator<CollisionResult>(results, [this, removed](const CollisionResult&, size_t snapshotIndex) {
		size_t index = snapshotIndex - *removed;
		if (index >= results.size())
			return false;
		results.erase(results.begin() + static_cast<std::ptrdiff_t>(index));
		++*removed;
		return true;
	});
}

std::vector<CollisionResult>::const_iterator CollisionResults::begin() {
	sortIfNeeded();
	return results.cbegin();
}

void CollisionResults::addCollision(const CollisionResult& result) {
	if (math::JavaFloat::isNaN(result.getDistance())) {
		return;
	}
	results.push_back(result);
	if (!onlyFirst)
		sorted = false;
}

int32_t CollisionResults::size() {
	return static_cast<int32_t>(results.size());
}

std::optional<CollisionResult> CollisionResults::getClosestCollision() {
	if (size() == 0)
		return std::nullopt;

	sortIfNeeded();

	return results.front();
}

std::optional<CollisionResult> CollisionResults::getFarthestCollision() {
	if (size() == 0)
		return std::nullopt;

	sortIfNeeded();

	return results.back();
}

CollisionResult CollisionResults::getCollision(int32_t index) {
	sortIfNeeded();

	checkIndex(index, results.size());
	return results[static_cast<size_t>(index)];
}

CollisionResult CollisionResults::getCollisionDirect(int32_t index) {
	checkIndex(index, results.size());
	return results[static_cast<size_t>(index)];
}

void CollisionResults::setGeometryDirect(int32_t index, runtime::Ptr<scene::Geometry> geometry) {
	checkIndex(index, results.size());
	results[static_cast<size_t>(index)].setGeometry(geometry);
}

std::string CollisionResults::toString() const {
	std::string sb;
	sb += "CollisionResults[";
	for (const CollisionResult& result : results) {
		// Java: Object.toString of CollisionResult (class name, '@', hex hashCode)
		sb += "com.aionemu.gameserver.geoEngine.collision.CollisionResult@" + std::format("{:x}", static_cast<uint32_t>(result.hashCode()));
		sb += ", ";
	}
	if (!results.empty())
		sb.resize(sb.size() - 2);

	sb += "]";
	return sb;
}

} // namespace aion::gameserver::geoEngine::collision
