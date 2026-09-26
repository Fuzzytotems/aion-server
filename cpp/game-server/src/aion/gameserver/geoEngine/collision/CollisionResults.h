#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::collision {

/**
 * The collisions found by one ray test, sorted by distance on access.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). K5 confined value class (fieldmap): created on the stack by the geo code and
 * returned by value (GeoService::getCollisions). C++ notes: the results are CollisionResult values; getClosestCollision/getFarthestCollision
 * return `std::optional` (Java null when empty), getCollision/getCollisionDirect a copy; Java `Iterable<CollisionResult>`: iterator() and
 * begin()/end() iterate the sorted results. The constructors are ported (member stores only).
 *
 * @author Kirill
 */
class CollisionResults {
private:
	/** players can't walk or stand on surfaces with >= 45° elevation angle (Java: Math.toRadians(45)) */
	static constexpr double SLOPING_SURFACE_ANGLE_RAD = 45.0 * 0.017453292519943295;

	std::vector<CollisionResult> results{};
	bool sorted = true;
	const int8_t intentions;
	const int32_t instanceId;
	const bool onlyFirst;
	const runtime::Ptr<IgnoreProperties> ignoreProperties;
	bool invalidateSlopingSurface = false;                  // confined: K5 value class (fieldmap)

public:
	CollisionResults(int8_t intentions, int32_t instanceId, runtime::Ptr<IgnoreProperties> ignoreProperties);

	CollisionResults(int8_t intentions, int32_t instanceId);

	CollisionResults(int8_t intentions, int32_t instanceId, bool searchFirst);

	CollisionResults(int8_t intentions, int32_t instanceId, bool searchFirst, runtime::Ptr<IgnoreProperties> ignoreProperties);

	void clear();

	/** Java: Iterator<CollisionResult> iterator() (sorts first) */
	runtime::JavaIterator<CollisionResult> iterator();

	/** C++ only: range-for over the sorted results (sorts first) */
	std::vector<CollisionResult>::const_iterator begin();

	std::vector<CollisionResult>::const_iterator end() const { return results.end(); }

	void addCollision(const CollisionResult& result);

	int32_t size();

	std::optional<CollisionResult> getClosestCollision();

	std::optional<CollisionResult> getFarthestCollision();

	CollisionResult getCollision(int32_t index);

	CollisionResult getCollisionDirect(int32_t index);

	/**
	 * C++ only: Java's `getCollisionDirect(index).setGeometry(geometry)` (Geometry.collideWith). getCollisionDirect returns a copy, so the geometry
	 * is set on the stored result here.
	 *
	 * @throws IndexOutOfBoundsException
	 */
	void setGeometryDirect(int32_t index, runtime::Ptr<scene::Geometry> geometry);

	std::string toString() const;

	bool isOnlyFirst() const { return onlyFirst; }

	int8_t getIntentions() const { return intentions; }

	int32_t getInstanceId() const { return instanceId; }

	runtime::Ptr<IgnoreProperties> getIgnoreProperties() const { return ignoreProperties; }

	bool shouldInvalidateSlopingSurface() const { return invalidateSlopingSurface; }

	double getSlopingSurfaceAngleRad() const { return SLOPING_SURFACE_ANGLE_RAD; }

	void setInvalidateSlopingSurface(bool value) { invalidateSlopingSurface = value; }

private:
	/** C++ only: Java's repeated `if (!sorted) { results.sort(null); sorted = true; }` */
	void sortIfNeeded();
};

} // namespace aion::gameserver::geoEngine::collision
