// Bit-exact comparison of the geo math port with golden values from oracle/gen_golden.py, a Python model of the Java float semantics.

#include <array>
#include <cstddef>

#include <gtest/gtest.h>

#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Matrix4f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/math/StrictMath.h"
#include "aion/gameserver/geoEngine/math/Vector2f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"

#include "GeoMathTestSupport.h"
#include "GoldenVectors.gen.h"

using namespace aion::gameserver::geoEngine::math;
using namespace aion::gameserver::geoEngine::math::test;

TEST(GoldenVectorsTest, FastMathConstants) {
	EXPECT_EQ(bitsOf(FastMath::PI), golden::CONST_PI);
	EXPECT_EQ(bitsOf(FastMath::TWO_PI), golden::CONST_TWO_PI);
	EXPECT_EQ(bitsOf(FastMath::HALF_PI), golden::CONST_HALF_PI);
	EXPECT_EQ(bitsOf(FastMath::QUARTER_PI), golden::CONST_QUARTER_PI);
	EXPECT_EQ(bitsOf(FastMath::INV_PI), golden::CONST_INV_PI);
	EXPECT_EQ(bitsOf(FastMath::INV_TWO_PI), golden::CONST_INV_TWO_PI);
	EXPECT_EQ(bitsOf(FastMath::DEG_TO_RAD), golden::CONST_DEG_TO_RAD);
	EXPECT_EQ(bitsOf(FastMath::RAD_TO_DEG), golden::CONST_RAD_TO_DEG);
	EXPECT_EQ(bitsOf(FastMath::ONE_THIRD), golden::CONST_ONE_THIRD);
	EXPECT_EQ(bitsOf(FastMath::FLOAT_EPSILON), golden::CONST_FLOAT_EPSILON);
	EXPECT_EQ(bitsOf(FastMath::ZERO_TOLERANCE), golden::CONST_ZERO_TOLERANCE);
}

TEST(GoldenVectorsTest, Vector3f) {
	size_t index = 0;
	for (const auto& c : golden::Vector3fCases) {
		SCOPED_TRACE(index++);
		const Vector3f a = vec3(c.a);
		const Vector3f b = vec3(c.b);
		const float s = fl(c.s);
		const float t = fl(c.t);

		EXPECT_FLOAT_BITS(a.dot(b), fl(c.dot));
		EXPECT_TRUE(sameVector(a.cross(b), c.cross));
		Vector3f crossStore(9, 9, 9);
		EXPECT_EQ(&a.cross(b, crossStore), &crossStore);
		EXPECT_TRUE(sameVector(crossStore, c.cross));
		Vector3f crossLocal = a;
		EXPECT_TRUE(sameVector(crossLocal.crossLocal(b), c.cross));
		Vector3f crossAliased = a;
		crossAliased.cross(b, crossAliased); // result aliases this, like Java
		EXPECT_TRUE(sameVector(crossAliased, c.cross));

		EXPECT_FLOAT_BITS(a.length(), fl(c.length));
		EXPECT_FLOAT_BITS(a.lengthSquared(), fl(c.lengthSquared));
		EXPECT_FLOAT_BITS(a.distance(b), fl(c.distance));
		EXPECT_FLOAT_BITS(a.distanceSquared(b), fl(c.distanceSquared));

		EXPECT_TRUE(sameVector(a.normalize(), c.normalize));
		Vector3f normalized = a;
		EXPECT_TRUE(sameVector(normalized.normalizeLocal(), c.normalize));

		EXPECT_TRUE(sameVector(a.divide(s), c.divide));
		Vector3f divided = a;
		EXPECT_TRUE(sameVector(divided.divideLocal(s), c.divide));

		Vector3f scaleAdd = a;
		EXPECT_TRUE(sameVector(scaleAdd.scaleAdd(s, b), c.scaleAdd));
		Vector3f scaleAdd3;
		EXPECT_TRUE(sameVector(scaleAdd3.scaleAdd(s, a, b), c.scaleAdd));

		Vector3f interpolated = a;
		EXPECT_TRUE(sameVector(interpolated.interpolate(b, t), c.interpolate));
		Vector3f interpolated3(7, 7, 7);
		EXPECT_TRUE(sameVector(interpolated3.interpolate(a, b, t), c.interpolate));

		EXPECT_TRUE(sameVector(a.project(b), c.project));
		EXPECT_FLOAT_BITS(a.normalize().angleBetween(b.normalize()), fl(c.angleBetween));
		EXPECT_EQ(a.hashCode(), c.hashCode);

		Vector3f u, v;
		Vector3f::generateComplementBasis(u, v, a.normalize());
		EXPECT_TRUE(sameVector(u, c.basisU));
		EXPECT_TRUE(sameVector(v, c.basisV));
		Vector3f w = a;
		Vector3f u2(5, 5, 5), v2(6, 6, 6);
		Vector3f::generateOrthonormalBasis(u2, v2, w);
		EXPECT_TRUE(sameVector(w, c.normalize));
		EXPECT_TRUE(sameVector(u2, c.basisU));
		EXPECT_TRUE(sameVector(v2, c.basisV));
	}
}

TEST(GoldenVectorsTest, Vector2f) {
	size_t index = 0;
	for (const auto& c : golden::Vector2fCases) {
		SCOPED_TRACE(index++);
		const Vector2f a = vec2(c.a);
		const Vector2f b = vec2(c.b);
		EXPECT_FLOAT_BITS(a.dot(b), fl(c.dot));
		EXPECT_FLOAT_BITS(a.determinant(b), fl(c.determinant));
		EXPECT_TRUE(sameVector(a.cross(b), 0.0f, 0.0f, fl(c.determinant)));
		EXPECT_FLOAT_BITS(a.length(), fl(c.length));
		EXPECT_FLOAT_BITS(a.distanceSquared(b), fl(c.distanceSquared));
		EXPECT_FLOAT_BITS(a.distanceSquared(b.x, b.y), fl(c.distanceSquared));
		EXPECT_TRUE(sameVector(a.normalize(), c.normalize));
		Vector2f normalized = a;
		EXPECT_TRUE(sameVector(normalized.normalizeLocal(), c.normalize));
		EXPECT_FLOAT_BITS(a.getAngle(), fl(c.getAngle));
		EXPECT_FLOAT_BITS(a.angleBetween(b), fl(c.angleBetween));
		EXPECT_FLOAT_BITS(a.normalize().smallestAngleBetween(b.normalize()), fl(c.smallestAngleBetween));
		EXPECT_TRUE(sameVector(a.subtract(b.x, b.y), c.subtractXY));
		EXPECT_EQ(a.hashCode(), c.hashCode);
	}
}

TEST(GoldenVectorsTest, Matrix3f) {
	size_t index = 0;
	for (const auto& c : golden::Matrix3fCases) {
		SCOPED_TRACE(index++);
		const Matrix3f a = mat3(c.a);
		const Matrix3f b = mat3(c.b);
		const Vector3f v = vec3(c.v);

		EXPECT_TRUE(sameMatrix(a.mult(b), c.mult));
		Matrix3f product = b;
		a.mult(product, product); // product aliases mat, documented as safe in Java
		EXPECT_TRUE(sameMatrix(product, c.mult));
		Matrix3f local = a;
		EXPECT_TRUE(sameMatrix(local.multLocal(b), c.mult));

		EXPECT_TRUE(sameVector(a.mult(v), c.multVector));
		Vector3f aliased = v;
		a.mult(aliased, aliased);
		EXPECT_TRUE(sameVector(aliased, c.multVector));
		Vector3f multLocal = v;
		EXPECT_TRUE(sameVector(a.multLocal(multLocal), c.multVector));

		EXPECT_FLOAT_BITS(a.determinant(), fl(c.determinant));
		EXPECT_TRUE(sameMatrix(a.invert(), c.invert));
		Matrix3f inverted;
		EXPECT_TRUE(sameMatrix(a.invert(inverted), c.invert));
		Matrix3f invertedLocal = a;
		EXPECT_TRUE(sameMatrix(invertedLocal.invertLocal(), c.invert));
		EXPECT_TRUE(sameMatrix(a.adjoint(), c.adjoint));

		Matrix3f rotation;
		rotation.fromStartEndVectors(vec3(c.start), vec3(c.end));
		EXPECT_TRUE(sameMatrix(rotation, c.fromStartEnd));

		EXPECT_EQ(a.hashCode(), c.hashCode);
	}
}

TEST(GoldenVectorsTest, Matrix4f) {
	size_t index = 0;
	for (const auto& c : golden::Matrix4fCases) {
		SCOPED_TRACE(index++);
		const Matrix4f a = mat4(c.a);
		const Matrix4f b = mat4(c.b);
		const Vector3f v = vec3(c.v);

		EXPECT_TRUE(sameMatrix(a.mult(b), c.mult));
		Matrix4f product = b;
		a.mult(product, product);
		EXPECT_TRUE(sameMatrix(product, c.mult));
		Matrix4f local = a;
		EXPECT_TRUE(sameMatrix(local.multLocal(b), c.mult));

		EXPECT_TRUE(sameVector(a.mult(v), c.multVector));
		Vector3f aliased = v;
		a.mult(aliased, aliased); // BIHNode: inv.mult(r.getOrigin(), r.getOrigin())
		EXPECT_TRUE(sameVector(aliased, c.multVector));
		Vector3f normal = v;
		a.multNormal(normal, normal);
		EXPECT_TRUE(sameVector(normal, c.multNormal));
		Vector3f normalAcross;
		EXPECT_TRUE(sameVector(a.multNormalAcross(v, normalAcross), c.multNormalAcross));
		Vector3f proj;
		EXPECT_FLOAT_BITS(a.multProj(v, proj), fl(c.multProjW));
		EXPECT_TRUE(sameVector(proj, c.multProj));
		Vector3f across;
		EXPECT_TRUE(sameVector(a.multAcross(v, across), c.multAcross));

		std::array<float, 4> vec4 = {v.x, v.y, v.z, 1.0f};
		a.mult(vec4);
		EXPECT_TRUE(sameVector(Vector3f(vec4[0], vec4[1], vec4[2]), c.multProj));
		EXPECT_FLOAT_BITS(vec4[3], fl(c.multProjW));
		std::array<float, 4> vec4Across = {v.x, v.y, v.z, 1.0f};
		a.multAcross(vec4Across);
		EXPECT_TRUE(sameVector(Vector3f(vec4Across[0], vec4Across[1], vec4Across[2]), c.multAcross));

		EXPECT_FLOAT_BITS(a.determinant(), fl(c.determinant));
		if (c.invertible) {
			EXPECT_TRUE(sameMatrix(a.invert(), c.invert));
			Matrix4f inverted;
			EXPECT_TRUE(sameMatrix(a.invert(inverted), c.invert));
			Matrix4f invertedLocal = a;
			EXPECT_TRUE(sameMatrix(invertedLocal.invertLocal(), c.invert));
		} else {
			EXPECT_THROW(a.invert(), ArithmeticException);
			Matrix4f invertedLocal = a;
			EXPECT_TRUE(invertedLocal.invertLocal().equals(Matrix4f().zero()));
		}
		EXPECT_TRUE(sameMatrix(a.adjoint(), c.adjoint));
		EXPECT_EQ(a.hashCode(), c.hashCode);
	}
}

TEST(GoldenVectorsTest, GeometryTransformAndBihRayTransform) {
	size_t index = 0;
	for (const auto& c : golden::TransformCases) {
		SCOPED_TRACE(index++);
		// GeoWorldLoader: matrix3f.set(i, j, geo.getFloat()) row by row
		Matrix3f rotation;
		for (int32_t i = 0; i < 3; i++) {
			for (int32_t j = 0; j < 3; j++)
				rotation.set(i, j, fl(c.rotation[i * 3 + j]));
		}
		// Geometry.setTransform
		Matrix4f world;
		world.loadIdentity();
		world.setRotationMatrix(rotation);
		world.scale(vec3(c.scale));
		world.setTranslation(vec3(c.location));
		EXPECT_TRUE(sameMatrix(world, c.world));

		// BIHNode.intersectWhere: Matrix4f inv = worldMatrix.invert(); transform the ray in place, normalize its direction
		const Matrix4f inv = world.invert();
		EXPECT_TRUE(sameMatrix(inv, c.inverse));
		Ray r(vec3(c.point), vec3(c.direction));
		const Vector3f o = r.getOrigin();
		const Vector3f d = r.getDirection();
		inv.mult(r.getOrigin(), r.getOrigin());
		inv.multNormal(r.getDirection(), r.getDirection());
		EXPECT_TRUE(sameVector(r.getOrigin(), c.localPoint));
		EXPECT_TRUE(sameVector(r.getDirection(), c.localDirection));
		r.getDirection().normalizeLocal();
		EXPECT_TRUE(sameVector(r.getDirection(), c.localDirectionNormalized));
		r.setOrigin(o);
		r.setDirection(d);
		EXPECT_TRUE(sameVector(r.origin, c.point));
		EXPECT_TRUE(sameVector(r.direction, c.direction));

		EXPECT_TRUE(sameVector(world.mult(vec3(c.point)), c.worldPoint));
	}
}

TEST(GoldenVectorsTest, Ray) {
	size_t index = 0;
	int hits = 0;
	int quadHits = 0;
	for (const auto& c : golden::RayCases) {
		SCOPED_TRACE(index++);
		const Ray r(vec3(c.origin), vec3(c.direction));
		const Vector3f v0 = vec3(c.v0);
		const Vector3f v1 = vec3(c.v1);
		const Vector3f v2 = vec3(c.v2);

		EXPECT_FLOAT_BITS(r.intersects(v0, v1, v2), fl(c.t));

		Vector3f location(123.0f, 456.0f, 789.0f);
		EXPECT_EQ(r.intersectWhere(v0, v1, v2, location), c.hit);
		EXPECT_EQ(r.intersectWhere(v0, v1, v2), c.hit);
		if (c.hit) {
			hits++;
			EXPECT_TRUE(sameVector(location, c.location));
		} else {
			EXPECT_TRUE(sameVector(location, 123.0f, 456.0f, 789.0f));
		}

		Vector3f quad(1.0f, 2.0f, 3.0f);
		EXPECT_EQ(r.intersectWherePlanarQuad(v0, v1, v2, quad), c.quadHit);
		if (c.quadHit) {
			quadHits++;
			EXPECT_TRUE(sameVector(quad, c.quad));
		} else {
			EXPECT_TRUE(sameVector(quad, 1.0f, 2.0f, 3.0f));
		}

		EXPECT_FLOAT_BITS(r.distanceSquared(vec3(c.point)), fl(c.distanceSquared));
	}
	// the vectors exercise both outcomes
	EXPECT_GT(hits, 16);
	EXPECT_LT(hits, static_cast<int>(std::size(golden::RayCases)));
	EXPECT_GT(quadHits, hits);
}

TEST(GoldenVectorsTest, FastMath) {
	size_t index = 0;
	for (const auto& c : golden::FastMathCases) {
		SCOPED_TRACE(index++);
		const float x = fl(c.x);
		const float y = fl(c.y);
		const float z = fl(c.z);
		const float t = fl(c.t);
		EXPECT_FLOAT_BITS(FastMath::invSqrt(x), fl(c.invSqrt));
		EXPECT_FLOAT_BITS(FastMath::fastInvSqrt(x), fl(c.fastInvSqrt));
		EXPECT_FLOAT_BITS(FastMath::sqrt(x), fl(c.sqrt));
		EXPECT_FLOAT_BITS(FastMath::reduceSinAngle(y), fl(c.reduceSinAngle));
		EXPECT_FLOAT_BITS(FastMath::interpolateLinear(t, y, z), fl(c.interpolateLinear));
		EXPECT_FLOAT_BITS(FastMath::interpolateCatmullRom(t, x, fl(c.p[0]), fl(c.p[1]), fl(c.p[2]), fl(c.p[3])), fl(c.catmullRom));
		EXPECT_FLOAT_BITS(FastMath::acos(x), fl(c.acosX));
		EXPECT_FLOAT_BITS(FastMath::asin(x), fl(c.asinX));
		EXPECT_FLOAT_BITS(FastMath::acos(fl(c.unit)), fl(c.acosUnit));
		EXPECT_FLOAT_BITS(FastMath::asin(fl(c.unit)), fl(c.asinUnit));
		EXPECT_FLOAT_BITS(FastMath::atan(y), fl(c.atanY));
		EXPECT_FLOAT_BITS(FastMath::atan2(y, z), fl(c.atan2YZ));
		EXPECT_FLOAT_BITS(FastMath::atan2(x, y), fl(c.atan2XY));
		EXPECT_FLOAT_BITS(FastMath::copysign(y, z), fl(c.copysign));
		EXPECT_FLOAT_BITS(FastMath::normalize(fl(c.angle), -FastMath::PI, FastMath::PI), fl(c.normalize));
		EXPECT_FLOAT_BITS(FastMath::determinant(db(c.m[0]), db(c.m[1]), db(c.m[2]), db(c.m[3]), db(c.m[4]), db(c.m[5]), db(c.m[6]), db(c.m[7]),
		                                        db(c.m[8]), db(c.m[9]), db(c.m[10]), db(c.m[11]), db(c.m[12]), db(c.m[13]), db(c.m[14]), db(c.m[15])),
		                  fl(c.determinant));
		EXPECT_FLOAT_BITS(FastMath::abs(x), fl(c.abs));
		EXPECT_FLOAT_BITS(FastMath::sign(x), fl(c.sign));
	}
}

TEST(GoldenVectorsTest, StrictMath) {
	size_t index = 0;
	for (const auto& c : golden::StrictMathCases) {
		SCOPED_TRACE(index++);
		const double x = db(c.x);
		const double y = db(c.y);
		EXPECT_DOUBLE_BITS(StrictMath::asin(x), db(c.asin));
		EXPECT_DOUBLE_BITS(StrictMath::acos(x), db(c.acos));
		EXPECT_DOUBLE_BITS(StrictMath::atan(x), db(c.atan));
		EXPECT_DOUBLE_BITS(StrictMath::atan2(y, x), db(c.atan2));
	}
}

TEST(GoldenVectorsTest, HalfFloatConversion) {
	size_t index = 0;
	for (const auto& c : golden::HalfCases) {
		SCOPED_TRACE(index++);
		EXPECT_EQ(FastMath::convertFloatToHalf(fl(c.value)), static_cast<int16_t>(c.half));
		EXPECT_FLOAT_BITS(FastMath::convertHalfToFloat(static_cast<int16_t>(c.half)), fl(c.back));
	}
}

TEST(GoldenVectorsTest, TrianglePredicates) {
	size_t index = 0;
	for (const auto& c : golden::TriangleCases) {
		SCOPED_TRACE(index++);
		EXPECT_EQ(FastMath::counterClockwise(vec2(c.t0), vec2(c.t1), vec2(c.p)), c.ccw);
		EXPECT_EQ(FastMath::pointInsideTriangle(vec2(c.t0), vec2(c.t1), vec2(c.t2), vec2(c.p)), c.inside);
	}
}
