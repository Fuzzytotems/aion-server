#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/skillengine/properties/fwd.h"
#include "aion/gameserver/utils/fwd.h"

namespace aion::gameserver::utils {

/**
 * Class with basic positional calculations.<br>
 * Thanks to:
 * <ul>
 * <li>http://geom-java.sourceforge.net/</li>
 * <li>http://local.wasp.uwa.edu.au/~pbourke/geometry/pointline/DistancePoint.java</li>
 * </ul>
 * <p>
 * C++: a static-only class (fieldmap K5). The float and double arithmetic follows Java's promotion rules exactly (float differences widened to
 * double only where Java widens), Math.atan2 is StrictMath.atan2 (fdlibm, bit-exact), Math.toDegrees/toRadians multiply by the JDK 9+ constants.
 * Deviation: Math.sin/cos use the C runtime (HotSpot's intrinsics are not bit-specified either, results may differ in the last bit).
 * getClosestPointOnSegment returns the Point2D by value (Java: a new object), since Point2D is a template class without identity here.
 *
 * @author Disturbing, SoulKeeper, ATracer, Wakizashi, Neon
 */
class PositionUtil {
private:
	static constexpr float MAX_ANGLE_DIFF = 90.0f;

public:
	PositionUtil() = delete;

	/** @return True if the object is behind the target. */
	static bool isBehind(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& target);

	/** @return True if the object is behind the target inside maxAngleDiff (e.g. ±90 degrees, meaning effective 180 degree coverage). */
	static bool isBehind(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& target, float maxAngleDiff);

	/** @return True if the object is in front of the target. */
	static bool isInFrontOf(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& target);

	/** @return True if the object is in front of the target inside maxAngleDiff (e.g. ±90 degrees, meaning effective 180 degree coverage). */
	static bool isInFrontOf(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& target, float maxAngleDiff);

private:
	/**
	 * @return True if both angles are within ±maxAngleDiff degrees of each other. The shortest distance between both angles will be checked, so
	 *         the effective difference between 345° and 5° will be 20 degrees instead of 340.
	 */
	static bool checkAngleDiff(float angle1, float angle2, float maxAngleDiff);

public:
	/**
	 * Calculates the angle where the target is located, relative to object's heading.<br>
	 * 0 degrees means directly looking at target and ±180 degrees means target stands behind object
	 *
	 * <pre>
	 *       0 (head view)
	 *  -90     90
	 *      ±180  (back)
	 * </pre>
	 */
	static float calculateAngleTowards(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& target);

	/** @see #calculateAngleTowards(VisibleObject, VisibleObject) */
	static float calculateAngleTowards(float x, float y, int8_t heading, float targetX, float targetY);

	/** Get an angle between the line defined by two points and the horizontal axis */
	static float calculateAngleFrom(float obj1X, float obj1Y, float obj2X, float obj2Y);

	/** Get an angle between the line defined by two objects and the horizontal axis */
	static float calculateAngleFrom(model::gameobjects::VisibleObject& obj1, model::gameobjects::VisibleObject& obj2);

	/** @return Angle in degrees */
	static float convertHeadingToAngle(int8_t clientHeading);

	/** @return clientHeading */
	static int8_t convertAngleToHeading(float angle);

	/** @return The heading for the specified x and y coordinates to look towards the specified x2 and y2 coordinates. */
	static int8_t getHeadingTowards(float x, float y, float x2, float y2);

	/** @return The heading for obj1 to look towards the specified x and y coordinates. */
	static int8_t getHeadingTowards(model::gameobjects::VisibleObject& obj1, float x, float y);

	/** @return The heading for obj1 to look towards obj2. */
	static int8_t getHeadingTowards(model::gameobjects::VisibleObject& obj1, model::gameobjects::VisibleObject& obj2);

	static float getDirectionalBound(model::gameobjects::VisibleObject& object1, model::gameobjects::VisibleObject& object2, bool inverseTarget);

	static float getDirectionalBound(model::gameobjects::VisibleObject& object1, model::gameobjects::VisibleObject& object2);

	/** @return The distance between two points (2D coordinates) */
	static double getDistance(float x1, float y1, float x2, float y2);

	/**
	 * Returns distance between two 3D points
	 *
	 * @return distance between points, 0 if one of them is null
	 */
	static double getDistance(runtime::Ptr<model::geometry::Point3D> point1, runtime::Ptr<model::geometry::Point3D> point2);

	static double getDistance(model::gameobjects::VisibleObject& object, float x, float y, float z);

	static double getDistance(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& object2);

	/**
	 * @return The distance between two objects. If centerToCenter is false, the dimensions of both objects are considered (distance between both
	 *         objects bound radius instead of the center).
	 */
	static double getDistance(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& object2, bool centerToCenter);

	/** @return The distance between two points (3D coordinates) */
	static double getDistance(float x1, float y1, float z1, float x2, float y2, float z2);

	/** @return True if two visible objects are within the given range of each other. */
	static bool isInRange(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& object2, float range);

	/**
	 * @return True if objects are in the given range of each other. If centerToCenter is false, the dimensions of both objects are considered
	 *         (distance between both objects bound radius instead of the center).
	 */
	static bool isInRange(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& object2, float range, bool centerToCenter);

	static bool isInRange(model::gameobjects::VisibleObject& obj, float x, float y, float z, float range);

	static bool isInRange(float x1, float y1, float z1, float x2, float y2, float z2, float range);

	static bool isInRangeLimited(model::gameobjects::VisibleObject& object1, model::gameobjects::VisibleObject& object2, float minRange,
		float maxRange);

	/** @return false if the attacker or the target is null */
	static bool isInAttackRange(runtime::Ptr<model::gameobjects::Creature> attacker, runtime::Ptr<model::gameobjects::Creature> target, float range);

	static float calculateMaxCoveredDistance(model::gameobjects::Creature& creature, int64_t movementDurationMillis);

private:
	static float calculateMaxDistanceOffset(model::gameobjects::Creature& creature);

public:
	static bool isInTalkRange(model::gameobjects::Creature& creature, model::gameobjects::Npc& npc);

	static bool isInTalkRange(model::gameobjects::Creature& creature, model::gameobjects::HouseObject& houseObject);

	/**
	 * This method tests if {@code obj2} is within a cylinder, originating from {@code obj1} with the given {@code length}. Source: <a
	 * href="http://www.flipcode.com/archives/Fast_Point-In-Cylinder_Test.shtml">link</a>
	 */
	static bool isInsideAttackCylinder(model::gameobjects::VisibleObject& obj1, model::gameobjects::VisibleObject& obj2, float length, float radius,
		skillengine::properties::AreaDirections direction);

	/**
	 * Returns closest point on segment to point
	 *
	 * @return closets point on segment to point
	 * @throws IllegalArgumentException
	 *           if the segment start equals the segment end
	 */
	static model::templates::zone::Point2D getClosestPointOnSegment(float sx1, float sy1, float sx2, float sy2, float px, float py);

	/** @return Normalized angle between 0 (inclusive) and 360 (exclusive) degrees. */
	static float normalizeAngle(float angle);
};

} // namespace aion::gameserver::utils
