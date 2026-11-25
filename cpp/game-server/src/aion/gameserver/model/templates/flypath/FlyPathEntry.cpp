#include "aion/gameserver/model/templates/flypath/FlyPathEntry.h"

#include <algorithm>

#include "aion/gameserver/model/geometry/Point3D.h"

namespace aion::gameserver::model::templates::flypath {

// Java FlyPathEntry.java:103-113 (windstreamPoints.getLast() of an empty list throws NoSuchElementException; C++ callers check isWindstream)
runtime::Ref<geometry::Point3D> FlyPathEntry::getWindstreamPosition(int32_t progress) const {
	int32_t index = std::max(0, progress) / 1000;
	if (index >= static_cast<int32_t>(windstreamPoints.size()) - 1) {
		const WindstreamPoint& end = windstreamPoints.back();
		return geometry::Point3D::create(end.getX(), end.getY(), end.getZ());
	}
	float ratio = static_cast<float>(std::max(0, progress) % 1000) / 1000.0f;
	const WindstreamPoint& from = windstreamPoints[static_cast<size_t>(index)];
	const WindstreamPoint& to = windstreamPoints[static_cast<size_t>(index) + 1];
	return geometry::Point3D::create(from.getX() + (to.getX() - from.getX()) * ratio, from.getY() + (to.getY() - from.getY()) * ratio,
		from.getZ() + (to.getZ() - from.getZ()) * ratio);
}

// Java FlyPathEntry.java:119-122
bool FlyPathEntry::canEnterWindstream(int32_t progress) const {
	int32_t index = std::max(0, progress) / 1000;
	return index + 1 >= static_cast<int32_t>(windstreamPoints.size()) || windstreamPoints[static_cast<size_t>(index)].canEnter() ||
		windstreamPoints[static_cast<size_t>(index) + 1].canEnter();
}

// Java FlyPathEntry.java:128-131
bool FlyPathEntry::canLeaveWindstream(int32_t progress) const {
	int32_t index = std::max(0, progress) / 1000;
	return index + 1 >= static_cast<int32_t>(windstreamPoints.size()) || windstreamPoints[static_cast<size_t>(index)].canExit() ||
		windstreamPoints[static_cast<size_t>(index) + 1].canExit();
}

} // namespace aion::gameserver::model::templates::flypath
