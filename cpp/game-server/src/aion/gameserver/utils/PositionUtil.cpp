#include "aion/gameserver/utils/PositionUtil.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/StrictMath.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/geometry/Point2DFactory.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/properties/AreaDirections.h"

namespace aion::gameserver::utils {

namespace {

using model::gameobjects::Creature;
using model::gameobjects::VisibleObject;

/** Java: Math.toDegrees (JDK 9+: angrad * RADIANS_TO_DEGREES) */
double toDegrees(double angrad) noexcept {
	return angrad * 57.29577951308232;
}

/** Java: Math.toRadians (JDK 9+: angdeg * DEGREES_TO_RADIANS) */
double toRadians(double angdeg) noexcept {
	return angdeg * 0.017453292519943295;
}

/** Java: the (int) cast of a float - NaN becomes 0, out-of-range values saturate */
int32_t floatToInt(float value) noexcept {
	return geoEngine::math::JavaFloat::doubleToInt(static_cast<double>(value));
}

const model::templates::BoundRadius& boundRadiusOf(VisibleObject& object) {
	return *object.getObjectTemplate()->getBoundRadius();
}

} // namespace

bool PositionUtil::isBehind(VisibleObject& object, VisibleObject& target) {
	return isBehind(object, target, MAX_ANGLE_DIFF);
}

bool PositionUtil::isBehind(VisibleObject& object, VisibleObject& target, float maxAngleDiff) {
	float angle1 = calculateAngleFrom(object, target);
	float angle2 = convertHeadingToAngle(target.getHeading());
	return checkAngleDiff(angle1, angle2, maxAngleDiff);
}

bool PositionUtil::isInFrontOf(VisibleObject& object, VisibleObject& target) {
	return isInFrontOf(object, target, MAX_ANGLE_DIFF);
}

bool PositionUtil::isInFrontOf(VisibleObject& object, VisibleObject& target, float maxAngleDiff) {
	if (maxAngleDiff >= 180)
		return true;
	float angle1 = calculateAngleFrom(target, object);
	float angle2 = convertHeadingToAngle(target.getHeading());
	return checkAngleDiff(angle1, angle2, maxAngleDiff);
}

bool PositionUtil::checkAngleDiff(float angle1, float angle2, float maxAngleDiff) {
	float angleDiff = geoEngine::math::JavaFloat::mathAbs(angle1 - angle2);
	if (angleDiff > 180)
		angleDiff -= 360;
	return geoEngine::math::JavaFloat::mathAbs(angleDiff) <= maxAngleDiff;
}

float PositionUtil::calculateAngleTowards(VisibleObject& object, VisibleObject& target) {
	return calculateAngleTowards(object.getX(), object.getY(), object.getHeading(), target.getX(), target.getY());
}

float PositionUtil::calculateAngleTowards(float x, float y, int8_t heading, float targetX, float targetY) {
	float angle1 = convertHeadingToAngle(heading);
	float angle2 = calculateAngleFrom(x, y, targetX, targetY);
	float angleDiff = angle1 - angle2;
	if (angleDiff < -180)
		angleDiff += 360;
	else if (angleDiff > 180)
		angleDiff -= 360;
	return angleDiff;
}

float PositionUtil::calculateAngleFrom(float obj1X, float obj1Y, float obj2X, float obj2Y) {
	float dy = obj2Y - obj1Y; // float subtraction, then widened like Java's Math.atan2(double, double) arguments
	float dx = obj2X - obj1X;
	float angleTarget = static_cast<float>(toDegrees(geoEngine::math::StrictMath::atan2(dy, dx)));
	return normalizeAngle(angleTarget);
}

float PositionUtil::calculateAngleFrom(VisibleObject& obj1, VisibleObject& obj2) {
	return calculateAngleFrom(obj1.getX(), obj1.getY(), obj2.getX(), obj2.getY());
}

float PositionUtil::convertHeadingToAngle(int8_t clientHeading) {
	return normalizeAngle(static_cast<float>(clientHeading) * 3.0f);
}

int8_t PositionUtil::convertAngleToHeading(float angle) {
	return static_cast<int8_t>(floatToInt(angle / 3));
}

int8_t PositionUtil::getHeadingTowards(float x, float y, float x2, float y2) {
	return convertAngleToHeading(calculateAngleFrom(x, y, x2, y2));
}

int8_t PositionUtil::getHeadingTowards(VisibleObject& obj1, float x, float y) {
	return getHeadingTowards(obj1.getX(), obj1.getY(), x, y);
}

int8_t PositionUtil::getHeadingTowards(VisibleObject& obj1, VisibleObject& obj2) {
	return getHeadingTowards(obj1, obj2.getX(), obj2.getY());
}

float PositionUtil::getDirectionalBound(VisibleObject& object1, VisibleObject& object2, bool inverseTarget) {
	float angle = 90 - (inverseTarget ? calculateAngleTowards(object2, object1) : calculateAngleTowards(object1, object2));
	double radians = toRadians(angle);
	float x1 = static_cast<float>(object1.getX() + boundRadiusOf(object1).getSide() * std::cos(radians));
	float y1 = static_cast<float>(object1.getY() + boundRadiusOf(object1).getFront() * std::sin(radians));
	float x2 = static_cast<float>(object2.getX() + boundRadiusOf(object2).getSide() * std::cos(std::numbers::pi + radians));
	float y2 = static_cast<float>(object2.getY() + boundRadiusOf(object2).getFront() * std::sin(std::numbers::pi + radians));
	float bound1 = static_cast<float>(getDistance(object1.getX(), object1.getY(), x1, y1));
	float bound2 = static_cast<float>(getDistance(object2.getX(), object2.getY(), x2, y2));
	return bound1 - bound2;
}

float PositionUtil::getDirectionalBound(VisibleObject& object1, VisibleObject& object2) {
	return getDirectionalBound(object1, object2, false);
}

double PositionUtil::getDistance(float x1, float y1, float x2, float y2) {
	float dx = x2 - x1;
	float dy = y2 - y1;
	return std::sqrt(static_cast<double>(dx * dx + dy * dy));
}

double PositionUtil::getDistance(runtime::Ptr<model::geometry::Point3D> point1, runtime::Ptr<model::geometry::Point3D> point2) {
	if (!point1 || !point2)
		return 0;
	return getDistance(point1->getX(), point1->getY(), point1->getZ(), point2->getX(), point2->getY(), point2->getZ());
}

double PositionUtil::getDistance(VisibleObject& object, float x, float y, float z) {
	return getDistance(object.getX(), object.getY(), object.getZ(), x, y, z);
}

double PositionUtil::getDistance(VisibleObject& object, VisibleObject& object2) {
	return getDistance(object, object2, true);
}

double PositionUtil::getDistance(VisibleObject& object, VisibleObject& object2, bool centerToCenter) {
	double distance = getDistance(object.getX(), object.getY(), object.getZ(), object2.getX(), object2.getY(), object2.getZ());
	if (!centerToCenter) {
		distance -= boundRadiusOf(object).getMaxOfFrontAndSide();
		distance -= boundRadiusOf(object2).getMaxOfFrontAndSide();
		if (distance < 0)
			distance = 0;
	}
	return distance;
}

double PositionUtil::getDistance(float x1, float y1, float z1, float x2, float y2, float z2) {
	float dx = x1 - x2;
	float dy = y1 - y2;
	float dz = z1 - z2;
	// We should avoid Math.pow or Math.hypot due to performance reasons
	return std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz));
}

bool PositionUtil::isInRange(VisibleObject& object, VisibleObject& object2, float range) {
	return isInRange(object, object2, range, true);
}

bool PositionUtil::isInRange(VisibleObject& object, VisibleObject& object2, float range, bool centerToCenter) {
	if (object.getWorldId() != object2.getWorldId() || object.getInstanceId() != object2.getInstanceId())
		return false;
	if (!centerToCenter) {
		range += boundRadiusOf(object).getMaxOfFrontAndSide();
		range += boundRadiusOf(object2).getMaxOfFrontAndSide();
	}
	return isInRange(object.getX(), object.getY(), object.getZ(), object2.getX(), object2.getY(), object2.getZ(), range);
}

bool PositionUtil::isInRange(VisibleObject& obj, float x, float y, float z, float range) {
	return isInRange(obj.getX(), obj.getY(), obj.getZ(), x, y, z, range);
}

bool PositionUtil::isInRange(float x1, float y1, float z1, float x2, float y2, float z2, float range) {
	float dx = x1 - x2;
	float dy = y1 - y2;
	float dz = z1 - z2;
	return dx * dx + dy * dy + dz * dz < range * range;
}

bool PositionUtil::isInRangeLimited(VisibleObject& object1, VisibleObject& object2, float minRange, float maxRange) {
	if (object1.getWorldId() != object2.getWorldId() || object1.getInstanceId() != object2.getInstanceId())
		return false;
	float dx = object2.getX() - object1.getX();
	float dy = object2.getY() - object1.getY();
	float dz = object2.getZ() - object1.getZ();
	float distSquared = dx * dx + dy * dy + dz * dz;
	return !(distSquared < minRange * minRange || distSquared > maxRange * maxRange);
}

bool PositionUtil::isInAttackRange(runtime::Ptr<Creature> attacker, runtime::Ptr<Creature> target, float range) {
	if (!attacker || !target)
		return false;
	if (attacker->getMoveController()->isInMove()) {
		float offset = calculateMaxDistanceOffset(*attacker);
		if (runtime::as<model::gameobjects::player::Player>(attacker))
			offset *= 1.33f; // client sends inaccurate coordinates during movement (they're always behind actual position...)
		range += offset;
	}
	if (target->getMoveController()->isInMove() && !runtime::as<model::gameobjects::Npc>(attacker))
		range += calculateMaxDistanceOffset(*target);
	return isInRange(*attacker, *target, range, false);
}

float PositionUtil::calculateMaxCoveredDistance(Creature& creature, int64_t movementDurationMillis) {
	if (movementDurationMillis <= 0)
		return 0;
	int32_t metersPerSecondInThousands = creature.getGameStats()->getMovementSpeed()->getCurrent();
	return static_cast<float>(static_cast<int64_t>(metersPerSecondInThousands) * movementDurationMillis) / 1'000'000.0f;
}

float PositionUtil::calculateMaxDistanceOffset(Creature& creature) {
	float offset = controllers::movement::CreatureMoveController::MOVE_CHECK_OFFSET;
	int64_t lastMove = creature.getMoveController()->getLastMoveUpdate();
	if (lastMove > 0) {
		// cap ms to avoid huge atk ranges during lags
		int64_t msSinceLastMove = std::min<int64_t>(1000, commons::utils::currentTimeMillis() - lastMove);
		offset += calculateMaxCoveredDistance(creature, msSinceLastMove);
	}
	return offset;
}

bool PositionUtil::isInTalkRange(Creature& creature, model::gameobjects::Npc& npc) {
	float range = static_cast<float>(npc.getObjectTemplate()->getTalkDistance() + 1);
	return isInRange(npc, creature, range, false);
}

bool PositionUtil::isInTalkRange(Creature& creature, model::gameobjects::HouseObject& houseObject) {
	float range = houseObject.getObjectTemplate()->getTalkingDistance() + 1;
	return isInRange(houseObject, creature, range, false);
}

bool PositionUtil::isInsideAttackCylinder(VisibleObject& obj1, VisibleObject& obj2, float length, float radius,
	skillengine::properties::AreaDirections direction) {
	double radian = toRadians(convertHeadingToAngle(obj1.getHeading()));
	if (direction == skillengine::properties::AreaDirections::BACK)
		radian += std::numbers::pi;
	length += boundRadiusOf(obj1).getFront() + boundRadiusOf(obj2).getFront();
	radius += boundRadiusOf(obj2).getFront();
	float dx = static_cast<float>(std::cos(radian) * length);
	float dy = static_cast<float>(std::sin(radian) * length);
	float tdx = obj2.getX() - obj1.getX();
	float tdy = obj2.getY() - obj1.getY();
	float tdz = obj2.getZ() - obj1.getZ();
	float lengthSqr = length * length;
	float dot = tdx * dx + tdy * dy;
	if (dot < 0.0f || dot > lengthSqr)
		return false;
	// distance squared to the cylinder axis
	return (tdx * tdx + tdy * tdy + tdz * tdz) - (dot * dot / lengthSqr) <= (radius * radius);
}

model::templates::zone::Point2D PositionUtil::getClosestPointOnSegment(float sx1, float sy1, float sx2, float sy2, float px, float py) {
	double xDelta = sx2 - sx1;
	double yDelta = sy2 - sy1;
	if ((xDelta == 0) && (yDelta == 0))
		throw runtime::IllegalArgumentException("Segment start equals segment end");
	double u = ((px - sx1) * xDelta + (py - sy1) * yDelta) / (xDelta * xDelta + yDelta * yDelta);
	if (u < 0)
		return model::geometry::makePoint2D(sx1, sy1);
	if (u > 1)
		return model::geometry::makePoint2D(sx2, sy2);
	return model::geometry::makePoint2D(static_cast<float>(sx1 + u * xDelta), static_cast<float>(sy1 + u * yDelta));
}

float PositionUtil::normalizeAngle(float angle) {
	if (angle >= 360) {
		angle = std::fmod(angle, 360.0f);
	} else if (angle < 0) {
		if (angle <= -360)
			angle = std::fmod(angle, 360.0f);
		if (angle < 0)
			angle += 360;
	}
	return angle;
}

} // namespace aion::gameserver::utils
