// P4-05 model.geometry: Polygon2D (java.awt.geom crossing rules), the areas and Point3D, with hand-derived expectations from the Java sources.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/geometry/CylinderArea.h"
#include "aion/gameserver/model/geometry/Plane3D.h"
#include "aion/gameserver/model/geometry/Point2DFactory.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/geometry/Polygon2D.h"
#include "aion/gameserver/model/geometry/RectangleArea.h"
#include "aion/gameserver/model/geometry/SemisphereArea.h"
#include "aion/gameserver/model/geometry/SphereArea.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::geometry {
namespace {

using templates::zone::Point2D;

runtime::Ref<Polygon2D> polygon(std::initializer_list<float> xs, std::initializer_list<float> ys) {
	std::vector<float> x(xs);
	std::vector<float> y(ys);
	return Polygon2D::create(std::span<const float>(x), std::span<const float>(y), static_cast<int32_t>(x.size()));
}

/** A U-shaped polygon: the notch x 10..20, y 10..30 is outside */
runtime::Ref<Polygon2D> uShape() {
	return polygon({0, 30, 30, 20, 20, 10, 10, 0}, {0, 0, 30, 30, 10, 10, 30, 30});
}

TEST(Polygon2DTest, ContainsFollowsPath2DInsideness) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<Polygon2D> square = polygon({0, 10, 10, 0}, {0, 0, 10, 10});
	EXPECT_TRUE(square->contains(5.0, 5.0));
	EXPECT_TRUE(square->contains(0.0, 0.0));   // the lower left corner is inside (half-open bounds, crossing at the right edge)
	EXPECT_FALSE(square->contains(10.0, 5.0)); // bounds are half-open: x < x0 + width
	EXPECT_FALSE(square->contains(5.0, 10.0));
	EXPECT_FALSE(square->contains(-1.0, 5.0));
	EXPECT_TRUE(square->contains(int32_t{3}, int32_t{4}));

	runtime::Ref<Polygon2D> u = uShape();
	EXPECT_TRUE(u->contains(5.0, 20.0));
	EXPECT_TRUE(u->contains(15.0, 5.0));
	EXPECT_FALSE(u->contains(15.0, 20.0));
	Polygon2D::Rectangle2D bounds = u->getBounds2D();
	EXPECT_TRUE(bounds.present);
	EXPECT_EQ(bounds.getMinX(), 0.0);
	EXPECT_EQ(bounds.getMaxY(), 30.0);

	runtime::Ref<Polygon2D> line = polygon({0, 10}, {0, 10}); // npoints <= 2: never contains
	EXPECT_FALSE(line->contains(5.0, 5.0));
}

TEST(Polygon2DTest, IntersectsAndContainsRectangles) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<Polygon2D> u = uShape();
	EXPECT_FALSE(u->intersects(12.0, 15.0, 6.0, 10.0)); // inside the notch
	EXPECT_TRUE(u->intersects(5.0, 5.0, 10.0, 3.0));    // inside the solid bottom
	EXPECT_TRUE(u->intersects(25.0, 5.0, 10.0, 5.0));   // crosses the right edge
	EXPECT_FALSE(u->intersects(40.0, 40.0, 5.0, 5.0));  // outside the bounds
	EXPECT_FALSE(u->intersects(5.0, 5.0, 0.0, 3.0));    // empty rectangle
	EXPECT_TRUE(u->contains(2.0, 2.0, 5.0, 5.0));
	EXPECT_FALSE(u->contains(12.0, 15.0, 6.0, 10.0));
	EXPECT_FALSE(u->contains(25.0, 5.0, 10.0, 5.0)); // RECT_INTERSECTS is not containment
	EXPECT_TRUE(u->intersects(Polygon2D::Rectangle2D{5.0, 5.0, 10.0, 3.0, true}));
}

TEST(Polygon2DTest, AddPointCloneAndRectangleConstructor) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<Polygon2D> built = Polygon2D::create();
	built->addPoint(0, 0);
	built->addPoint(10, 0);
	built->addPoint(10, 10);
	built->addPoint(0, 10);
	built->addPoint(-5, 5); // grows the arrays beyond the initial 4 slots
	EXPECT_EQ(built->npoints.get(), 5);
	EXPECT_EQ(built->xpoints.get()->length(), 8);
	EXPECT_TRUE(built->contains(-1.0, 5.0));
	EXPECT_EQ(built->getBounds2D().getMinX(), -5.0);
	EXPECT_EQ(built->getBounds2D().getWidth(), 15.0);

	runtime::Ref<Polygon2D> copy = built->clone();
	EXPECT_EQ(copy->npoints.get(), 5);
	EXPECT_TRUE(copy->contains(-1.0, 5.0));

	runtime::Ref<Polygon2D> fromRectangle = Polygon2D::create(Polygon2D::Rectangle2D{1.0, 2.0, 3.0, 4.0, true});
	EXPECT_EQ(fromRectangle->npoints.get(), 4);
	EXPECT_TRUE(fromRectangle->contains(2.0, 3.0));
	EXPECT_THROW(Polygon2D::create(Polygon2D::Rectangle2D{}), runtime::IndexOutOfBoundsException);

	std::array<float, 2> two{1, 2};
	EXPECT_THROW(Polygon2D::create(std::span<const float>(two), std::span<const float>(two), 3), runtime::IndexOutOfBoundsException);

	built->reset();
	EXPECT_EQ(built->npoints.get(), 0);
	EXPECT_THROW(built->addPoint(1, 1), runtime::IllegalStateException); // Java: lineTo on the empty GeneralPath of reset()
}

TEST(AreaTest, RectangleArea) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<RectangleArea> rect = RectangleArea::create(nullptr, 110010000, 0, 0, 10, 10, 0, 10);
	EXPECT_TRUE(rect->isInside2D(10.0f, 10.0f)); // inclusive
	EXPECT_FALSE(rect->isInside3D(5.0f, 5.0f, 11.0f));
	EXPECT_EQ(rect->getDistance2D(13.0f, 14.0f), 5.0);
	EXPECT_EQ(rect->getDistance3D(13.0f, 14.0f, 10.0f), 5.0);
	EXPECT_EQ(rect->getDistance3D(5.0f, 5.0f, 20.0f), 10.0);
	EXPECT_EQ(rect->getDistance2D(5.0f, 5.0f), 0.0);
	Point2D closest = rect->getClosestPoint2D(-3.0f, 4.0f);
	EXPECT_EQ(closest.getX(), 0.0f);
	EXPECT_EQ(closest.getY(), 4.0f);
	EXPECT_FALSE(rect->intersectsRectangle(*rect)); // Java: auto-generated stub
	EXPECT_EQ(rect->getWorldId(), 110010000);
	EXPECT_EQ(rect->getZoneName(), nullptr);
	runtime::Ref<Point3D> closest3d = rect->getClosestPoint(20.0f, 5.0f, -4.0f);
	EXPECT_EQ(closest3d->getX(), 10.0f);
	EXPECT_EQ(closest3d->getZ(), 0.0f);
	std::optional<Point2D> closestRight = rect->getClosestPoint(20.0f, 5.0f);
	ASSERT_TRUE(closestRight.has_value());
	EXPECT_EQ(closestRight->getX(), 10.0f);
	EXPECT_EQ(closestRight->getY(), 5.0f);
	Point2D insidePoint(3.0f, 4.0f);
	std::optional<Point2D> closestInside = rect->getClosestPoint(&insidePoint); // Java: the point's own coordinates if it is inside
	ASSERT_TRUE(closestInside.has_value());
	EXPECT_EQ(closestInside->getX(), 3.0f);
	EXPECT_EQ(closestInside->getY(), 4.0f);
}

TEST(AreaTest, AbstractAreaRejectsInvertedZ) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	try {
		RectangleArea::create(nullptr, 1, 0, 0, 1, 1, 10, 0.5f);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "minZ(10.0) > maxZ(0.5)");
	}
}

TEST(AreaTest, CylinderArea) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<CylinderArea> cylinder = CylinderArea::create(nullptr, 1, 0, 0, 5, 0, 10);
	EXPECT_TRUE(cylinder->isInside2D(3.0f, 0.0f));
	EXPECT_FALSE(cylinder->isInside2D(5.0f, 0.0f)); // strict
	EXPECT_EQ(cylinder->getDistance2D(8.0f, 0.0f), 3.0);
	EXPECT_EQ(cylinder->getDistance3D(0.0f, 0.0f, 15.0f), 5.0);
	EXPECT_EQ(cylinder->getDistance3D(0.0f, 8.0f, 5.0f), 3.0);
	Point2D onCircle = cylinder->getClosestPoint2D(0.0f, 10.0f);
	EXPECT_EQ(onCircle.getX(), 0.0f);
	EXPECT_EQ(onCircle.getY(), 5.0f);
	std::optional<Point2D> onCircleOverride = cylinder->getClosestPoint(0.0f, 10.0f);
	ASSERT_TRUE(onCircleOverride.has_value());
	EXPECT_EQ(onCircleOverride->getY(), 5.0f);
	runtime::Ref<RectangleArea> nearRect = RectangleArea::create(nullptr, 1, 4, -1, 14, 1, 0, 10);
	runtime::Ref<RectangleArea> farRect = RectangleArea::create(nullptr, 1, 6, -1, 14, 1, 0, 10);
	runtime::Ref<RectangleArea> aboveRect = RectangleArea::create(nullptr, 1, 4, -1, 14, 1, 11, 20);
	EXPECT_TRUE(cylinder->intersectsRectangle(*nearRect));
	EXPECT_FALSE(cylinder->intersectsRectangle(*farRect));
	EXPECT_FALSE(cylinder->intersectsRectangle(*aboveRect));
	Point2D center = makePoint2D(0, 0);
	runtime::Ref<CylinderArea> fromPoint = CylinderArea::create(nullptr, 1, &center, 5, 0, 10);
	EXPECT_TRUE(fromPoint->isInside3D(1.0f, 1.0f, 1.0f));
}

TEST(AreaTest, PolyArea) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	std::vector<Point2D> points{makePoint2D(0, 0), makePoint2D(10, 0), makePoint2D(10, 10), makePoint2D(0, 10)};
	runtime::Ref<PolyArea> area = PolyArea::create(nullptr, 1, points, 0, 10);
	EXPECT_TRUE(area->isInside2D(5.0f, 5.0f));
	EXPECT_EQ(area->getDistance2D(15.0f, 5.0f), 5.0);
	EXPECT_EQ(area->getDistance3D(15.0f, 5.0f, 5.0f), 5.0);
	// Java: the closest point of an inside point is on the first nearest edge, (5, 0), at the top z: sqrt(5 * 5 + 3 * 3)
	EXPECT_EQ(area->getDistance3D(5.0f, 5.0f, 13.0f), std::sqrt(34.0));
	Point2D closest = area->getClosestPoint2D(-2.0f, 12.0f);
	EXPECT_EQ(closest.getX(), 0.0f);
	EXPECT_EQ(closest.getY(), 10.0f);
	std::optional<Point2D> closestOverride = area->getClosestPoint(-2.0f, 12.0f);
	ASSERT_TRUE(closestOverride.has_value());
	EXPECT_EQ(closestOverride->getX(), 0.0f);
	EXPECT_EQ(closestOverride->getY(), 10.0f);

	configs::main::WorldConfig::WORLD_REGION_SIZE.store(10);
	runtime::Ref<RectangleArea> region = RectangleArea::create(nullptr, 1, 5, 5, 15, 15, 0, 10);
	runtime::Ref<RectangleArea> distantRegion = RectangleArea::create(nullptr, 1, 20, 20, 30, 30, 0, 10);
	EXPECT_TRUE(area->intersectsRectangle(*region));
	EXPECT_FALSE(area->intersectsRectangle(*distantRegion));
	configs::main::WorldConfig::WORLD_REGION_SIZE.store(0);

	std::vector<Point2D> twoPoints{makePoint2D(0, 0), makePoint2D(1, 1)};
	try {
		PolyArea::create(nullptr, 1, twoPoints, 0, 1);
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Not enough points, needed at least 3 but got 2");
	}
}

TEST(AreaTest, SphereAndSemisphere) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<SphereArea> sphere = SphereArea::create(nullptr, 7, 0, 0, 0, 5);
	runtime::Ref<Point3D> inside = Point3D::create(3.0f, 0.0f, 0.0f);
	EXPECT_TRUE(sphere->isInside3D(*inside));
	EXPECT_FALSE(sphere->isInside3D(5.0f, 0.0f, 0.0f));
	EXPECT_FALSE(sphere->isInside2D(0.0f, 0.0f)); // deprecated: always false
	EXPECT_EQ(sphere->getDistance2D(9.0f, 9.0f), 0.0);
	EXPECT_EQ(sphere->getDistance3D(8.0f, 0.0f, 0.0f), 3.0);
	EXPECT_EQ(sphere->getDistance3D(1.0f, 0.0f, 0.0f), 0.0);
	EXPECT_EQ(sphere->getMinZ(), -5.0f);
	EXPECT_TRUE(sphere->isInsideZ(4.0f));
	EXPECT_FALSE(sphere->getClosestPoint(1.0f, 1.0f).has_value()); // Java: null
	Point2D planePoint(1.0f, 1.0f);
	EXPECT_FALSE(sphere->getClosestPoint(&planePoint).has_value());
	EXPECT_EQ(sphere->getClosestPoint(*inside), nullptr);
	EXPECT_EQ(sphere->getClosestPoint(1.0f, 1.0f, 1.0f), nullptr);
	EXPECT_EQ(sphere->getWorldId(), 7);

	runtime::Ref<SemisphereArea> semisphere = SemisphereArea::create(nullptr, 7, 0, 0, 0, 5);
	EXPECT_FALSE(semisphere->isInside3D(3.0f, 0.0f, -1.0f));
	EXPECT_TRUE(semisphere->isInside3D(0.0f, 0.0f, 1.0f));
	EXPECT_EQ(semisphere->getMinZ(), 0.0f);
	EXPECT_EQ(semisphere->getMaxZ(), 5.0f);
	EXPECT_FALSE(semisphere->isInsideZ(-1.0f));
	EXPECT_EQ(semisphere->getDistance3D(0.0f, 0.0f, -8.0f), 3.0);
	EXPECT_EQ(semisphere->getDistance3D(0.0f, 0.0f, -2.0f), -3.0); // below the base the negative distance is returned
	EXPECT_EQ(semisphere->getDistance3D(0.0f, 0.0f, 2.0f), 0.0);

	runtime::Ref<RectangleArea> touching = RectangleArea::create(nullptr, 7, 4, -1, 14, 1, -10, 10);
	runtime::Ref<RectangleArea> below = RectangleArea::create(nullptr, 7, 4, -1, 14, 1, -10, -6);
	EXPECT_TRUE(sphere->intersectsRectangle(*touching));
	EXPECT_FALSE(sphere->intersectsRectangle(*below));
	EXPECT_TRUE(semisphere->intersectsRectangle(*touching));
}

TEST(Point3DTest, EqualsHashCodeCloneToString) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<Point3D> point = Point3D::create(1.0f, 2.0f, 3.0f);
	runtime::Ref<Point3D> same = Point3D::create(1.0, 2.0, 3.0);
	EXPECT_TRUE(point->equals(*same));
	EXPECT_FALSE(point->equals(*Point3D::create()));
	EXPECT_EQ(point->hashCode(), 102600); // ((1 * 31 + 2) * 31 + 3) * 100
	EXPECT_EQ(Point3D::create(0.5f, 0.25f, 0.0f)->hashCode(), 48825); // ((0.5 * 31 + 0.25) * 31 + 0) * 100
	runtime::Ref<Point3D> copy = point->clone();
	EXPECT_NE(copy, point);
	EXPECT_TRUE(copy->equals(*point));
	EXPECT_EQ(point->toString(), "Point3D{x=1.0, y=2.0, z=3.0}");
	EXPECT_EQ(Point3D::create(0.1f, -0.0f, 1e7f)->toString(), "Point3D{x=0.1, y=-0.0, z=1.0E7}");
}

TEST(Plane3DTest, Intersection) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	using geoEngine::math::Vector3f;
	runtime::Ref<Plane3D> ground = Plane3D::create(Vector3f(0, 0, 0), Vector3f(1, 0, 0), Vector3f(0, 1, 0));
	std::optional<Vector3f> hit = ground->intersection(Vector3f(2, 3, 4), Vector3f(2, 3, -4));
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->x, 2.0f);
	EXPECT_EQ(hit->y, 3.0f);
	EXPECT_EQ(hit->z, 0.0f);
	EXPECT_FALSE(ground->intersection(Vector3f(0, 0, 1), Vector3f(5, 5, 1)).has_value()); // parallel
	EXPECT_FALSE(ground->intersection(Vector3f(0, 0, 4), Vector3f(0, 0, 1)).has_value()); // plane beyond the segment end
}

} // namespace
} // namespace aion::gameserver::model::geometry
