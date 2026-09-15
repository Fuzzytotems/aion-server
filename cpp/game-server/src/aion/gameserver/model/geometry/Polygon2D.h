#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/geometry/fwd.h"

namespace aion::gameserver::model::geometry {

/**
 * This class is a Polygon with float coordinates.
 * <p>
 * Apache Batik's Polygon2D (Apache License 2.0). C++: RefCounted (fieldmap K4, PolyArea.poly) with the fieldmap members. Java delegates the
 * geometry to java.awt.geom (GeneralPath, Rectangle2D); the port keeps exactly the parts of that behaviour the class relies on:
 * <ul>
 * <li>Rectangle2D is a value struct with Rectangle2D.Double's contains/intersects/isEmpty (the bounds of a Path2D are computed in double since
 * JDK 19, as on the server's Java 25); `present` stands for Java's non-null reference.</li>
 * <li>Path2D describes a GeneralPath built from this polygon's own coordinates: the first `numPoints` points joined by moveTo/lineTo, the
 * winding rule and whether closePath() was appended. Every path Polygon2D creates is such a prefix, so the coordinates are read from xpoints and
 * ypoints (Java: copied into the path). contains(x, y), intersects and contains(rectangle) run java.awt.geom.Path2D's crossing algorithms
 * (Curve.pointCrossingsForLine, Curve.rectCrossingsForLine) on it.</li>
 * <li>Deviation: not ported (no caller, and they need java.awt types the port does not have): Polygon2D(java.awt.Polygon), getPolygon(),
 * getPolyline2D() with the package-private class Polyline2D, addPoint(java.awt.geom.Point2D), contains(java.awt.Point),
 * contains(java.awt.geom.Point2D), getBounds() (java.awt.Rectangle) and getPathIterator (DEVIATIONS).</li>
 * </ul>
 * Thread-safety: like Java, the lazily created closed path is a benign race (it is recomputed from the same points); the fields are torn-free.
 * java-race: addPoint and reset are not synchronized in Java either; PolyArea never calls them after construction.
 */
class Polygon2D : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** Java: java.awt.geom.Rectangle2D (Rectangle2D.Double semantics); present == false stands for null */
	struct Rectangle2D {
		double x = 0;
		double y = 0;
		double width = 0;
		double height = 0;
		bool present = false;

		double getX() const noexcept { return x; }
		double getY() const noexcept { return y; }
		double getWidth() const noexcept { return width; }
		double getHeight() const noexcept { return height; }
		double getMinX() const noexcept { return x; }
		double getMinY() const noexcept { return y; }
		double getMaxX() const noexcept { return x + width; }
		double getMaxY() const noexcept { return y + height; }

		/** Java: Rectangle2D.Double.isEmpty() */
		bool isEmpty() const noexcept { return width <= 0.0 || height <= 0.0; }

		/** Java: Rectangle2D.contains(double, double) */
		bool contains(double px, double py) const noexcept { return px >= x && py >= y && px < x + width && py < y + height; }

		/** Java: Rectangle2D.intersects(double, double, double, double) */
		bool intersects(double rx, double ry, double w, double h) const noexcept {
			if (isEmpty() || w <= 0 || h <= 0)
				return false;
			return rx + w > x && ry + h > y && rx < x + width && ry < y + height;
		}
	};

	/** Java: the GeneralPath fields path/closedPath (see the class comment); present == false stands for null */
	struct Path2D {
		int32_t numPoints = 0;
		bool windEvenOdd = false;
		bool closed = false;
		bool present = false;
	};

	/** The total number of points. The value of <code>npoints</code> represents the number of valid points in this <code>Polygon</code>. */
	runtime::Field<int32_t> npoints{};
	/** The array of <i>x</i> coordinates. The value of {@link #npoints npoints} is equal to the number of points in this <code>Polygon2D</code>. */
	runtime::Field<runtime::Ref<runtime::Array<float>>> xpoints{};
	/** The array of <i>x</i> coordinates. The value of {@link #npoints npoints} is equal to the number of points in this <code>Polygon2D</code>. */
	runtime::Field<runtime::Ref<runtime::Array<float>>> ypoints{};

protected:
	/** Bounds of the Polygon2D. */
	runtime::Field<Rectangle2D> bounds{};

private:
	runtime::Field<Path2D> path{};
	runtime::Field<Path2D> closedPath{};

protected:
	/** Creates an empty Polygon2D. */
	Polygon2D();

public:
	static runtime::Ref<Polygon2D> create();

protected:
	/**
	 * Constructs and initializes a <code>Polygon2D</code> from the specified Rectangle2D.
	 *
	 * @throws IndexOutOfBoundsException
	 *           rec is <code>null</code> (not present).
	 */
	explicit Polygon2D(const Rectangle2D& rec);

public:
	static runtime::Ref<Polygon2D> create(const Rectangle2D& rec);

protected:
	/**
	 * Constructs and initializes a <code>Polygon2D</code> from the specified parameters.
	 *
	 * @throws IndexOutOfBoundsException
	 *           if <code>npoints</code> is greater than the length of <code>xpoints</code> or the length of <code>ypoints</code>.
	 */
	Polygon2D(std::span<const float> xpoints, std::span<const float> ypoints, int32_t npoints);

public:
	static runtime::Ref<Polygon2D> create(std::span<const float> xpointsValue, std::span<const float> ypointsValue, int32_t npointsValue);

protected:
	/**
	 * Constructs and initializes a <code>Polygon2D</code> from the specified parameters.
	 *
	 * @throws IndexOutOfBoundsException
	 *           if <code>npoints</code> is greater than the length of <code>xpoints</code> or the length of <code>ypoints</code>.
	 */
	Polygon2D(std::span<const int32_t> xpoints, std::span<const int32_t> ypoints, int32_t npoints);

public:
	static runtime::Ref<Polygon2D> create(std::span<const int32_t> xpointsValue, std::span<const int32_t> ypointsValue, int32_t npointsValue);

	/** Resets this <code>Polygon</code> object to an empty polygon. */
	void reset();

	/** Java: clone() - a new polygon with the valid points of this one */
	runtime::Ref<Polygon2D> clone();

private:
	void calculatePath();

	void updatePath(float x, float y);

public:
	/** Appends the specified coordinates to this <code>Polygon2D</code>. */
	void addPoint(float x, float y);

	/** Determines whether the specified coordinates are inside this <code>Polygon</code>. */
	bool contains(int32_t x, int32_t y);

	/** Returns the high precision bounding box of the {@link Shape}. */
	Rectangle2D getBounds2D() const { return bounds.get(); }

	/**
	 * Determines if the specified coordinates are inside this <code>Polygon</code>. For the definition of <i>insideness</i>, see the class
	 * comments of {@link Shape}.
	 */
	bool contains(double x, double y);

private:
	void updateComputingPath();

public:
	/** Tests if the interior of this <code>Polygon</code> intersects the interior of a specified set of rectangular coordinates. */
	bool intersects(double x, double y, double w, double h);

	/** Tests if the interior of this <code>Polygon</code> intersects the interior of a specified <code>Rectangle2D</code>. */
	bool intersects(const Rectangle2D& r);

	/** Tests if the interior of this <code>Polygon</code> entirely contains the specified set of rectangular coordinates. */
	bool contains(double x, double y, double w, double h);

	/** Tests if the interior of this <code>Polygon</code> entirely contains the specified <code>Rectangle2D</code>. */
	bool contains(const Rectangle2D& r);

private:
	/** C++ only: Java GeneralPath(WIND_NON_ZERO) built by moveTo(xpoints[0], ypoints[0]) and lineTo for the other points up to npoints */
	static Path2D newPathOfPoints(int32_t numPoints, bool windEvenOdd) noexcept;

	/** C++ only: Java Path2D.Float.getBounds2D() of the path over the first numPoints points */
	Rectangle2D boundsOfPath(const Path2D& pathValue) const;

	/** C++ only: Java Path2D.Float.pointCrossings(px, py) of the path */
	int32_t pointCrossings(const Path2D& pathValue, double px, double py) const;

	/** C++ only: Java Path2D.Float.rectCrossings(rxmin, rymin, rxmax, rymax) of the path */
	int32_t rectCrossings(const Path2D& pathValue, double rxmin, double rymin, double rxmax, double rymax) const;

protected:
	~Polygon2D() override;
};

} // namespace aion::gameserver::model::geometry
