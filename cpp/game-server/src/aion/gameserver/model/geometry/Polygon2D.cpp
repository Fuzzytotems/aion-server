#include "aion/gameserver/model/geometry/Polygon2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::geometry {

namespace {

/** Java: Curve.RECT_INTERSECTS */
constexpr int32_t RECT_INTERSECTS = std::numeric_limits<int32_t>::min();

/** Java: IllegalPathStateException thrown by Path2D when a segment is appended to a path without an initial moveTo */
[[noreturn]] void throwMissingMoveTo() {
	throw runtime::IllegalStateException("missing initial moveto in path definition");
}

/** Java: Curve.pointCrossingsForLine */
int32_t pointCrossingsForLine(double px, double py, double x0, double y0, double x1, double y1) noexcept {
	if (py < y0 && py < y1)
		return 0;
	if (py >= y0 && py >= y1)
		return 0;
	// assert(y0 != y1);
	if (px >= x0 && px >= x1)
		return 0;
	if (px < x0 && px < x1)
		return (y0 < y1) ? 1 : -1;
	double xintercept = x0 + (py - y0) * (x1 - x0) / (y1 - y0);
	if (px >= xintercept)
		return 0;
	return (y0 < y1) ? 1 : -1;
}

/** Java: Curve.rectCrossingsForLine (int arithmetic wraps like Java's; RECT_INTERSECTS is never incremented) */
int32_t rectCrossingsForLine(int32_t crossings, double rxmin, double rymin, double rxmax, double rymax, double x0, double y0, double x1,
	double y1) noexcept {
	if (y0 >= rymax && y1 >= rymax)
		return crossings;
	if (y0 <= rymin && y1 <= rymin)
		return crossings;
	if (x0 <= rxmin && x1 <= rxmin)
		return crossings;
	if (x0 >= rxmax && x1 >= rxmax) {
		// Line is entirely to the right of the rect and the vertical ranges of the two overlap by a non-empty amount. Thus, this line segment is
		// partially in the "right-shadow". Path may have done a complete crossing or may have entered or exited the right-shadow.
		if (y0 < y1) {
			// y-increasing line segment... We know that y0 < rymax and y1 > rymin
			if (y0 <= rymin)
				crossings++;
			if (y1 >= rymax)
				crossings++;
		} else if (y1 < y0) {
			// y-decreasing line segment... We know that y1 < rymax and y0 > rymin
			if (y1 <= rymin)
				crossings--;
			if (y0 >= rymax)
				crossings--;
		}
		return crossings;
	}
	// Remaining case: both x and y ranges overlap by a non-empty amount. First do trivial INTERSECTS rejection of the cases where one of the
	// endpoints is inside the rectangle.
	if ((x0 > rxmin && x0 < rxmax && y0 > rymin && y0 < rymax) || (x1 > rxmin && x1 < rxmax && y1 > rymin && y1 < rymax))
		return RECT_INTERSECTS;
	// Otherwise calculate the y intercepts and see where they fall with respect to the rectangle
	double xi0 = x0;
	if (y0 < rymin)
		xi0 += ((rymin - y0) * (x1 - x0) / (y1 - y0));
	else if (y0 > rymax)
		xi0 += ((rymax - y0) * (x1 - x0) / (y1 - y0));
	double xi1 = x1;
	if (y1 < rymin)
		xi1 += ((rymin - y1) * (x0 - x1) / (y0 - y1));
	else if (y1 > rymax)
		xi1 += ((rymax - y1) * (x0 - x1) / (y0 - y1));
	if (xi0 <= rxmin && xi1 <= rxmin)
		return crossings;
	if (xi0 >= rxmax && xi1 >= rxmax) {
		if (y0 < y1) {
			if (y0 <= rymin)
				crossings++;
			if (y1 >= rymax)
				crossings++;
		} else if (y1 < y0) {
			if (y1 <= rymin)
				crossings--;
			if (y0 >= rymax)
				crossings--;
		}
		return crossings;
	}
	return RECT_INTERSECTS;
}

} // namespace

Polygon2D::Polygon2D() : xpoints(runtime::Array<float>::make(4)), ypoints(runtime::Array<float>::make(4)) {
}

runtime::Ref<Polygon2D> Polygon2D::create() {
	return runtime::makeRef<Polygon2D>();
}

Polygon2D::Polygon2D(const Rectangle2D& rec) {
	if (!rec.present)
		throw runtime::IndexOutOfBoundsException("null Rectangle");
	npoints = 4;
	runtime::Ref<runtime::Array<float>> xs = runtime::Array<float>::make(4);
	runtime::Ref<runtime::Array<float>> ys = runtime::Array<float>::make(4);
	(*xs)[0] = static_cast<float>(rec.getMinX());
	(*ys)[0] = static_cast<float>(rec.getMinY());
	(*xs)[1] = static_cast<float>(rec.getMaxX());
	(*ys)[1] = static_cast<float>(rec.getMinY());
	(*xs)[2] = static_cast<float>(rec.getMaxX());
	(*ys)[2] = static_cast<float>(rec.getMaxY());
	(*xs)[3] = static_cast<float>(rec.getMinX());
	(*ys)[3] = static_cast<float>(rec.getMaxY());
	xpoints = std::move(xs);
	ypoints = std::move(ys);
	calculatePath();
}

runtime::Ref<Polygon2D> Polygon2D::create(const Rectangle2D& rec) {
	return runtime::makeRef<Polygon2D>(rec);
}

Polygon2D::Polygon2D(std::span<const float> xpointsValue, std::span<const float> ypointsValue, int32_t npointsValue) {
	if (npointsValue > static_cast<int32_t>(xpointsValue.size()) || npointsValue > static_cast<int32_t>(ypointsValue.size()))
		throw runtime::IndexOutOfBoundsException("npoints > xpoints.length || npoints > ypoints.length");
	npoints = npointsValue;
	runtime::Ref<runtime::Array<float>> xs = runtime::Array<float>::make(npointsValue); // negative: NegativeArraySizeException in Java
	runtime::Ref<runtime::Array<float>> ys = runtime::Array<float>::make(npointsValue);
	for (int32_t i = 0; i < npointsValue; i++) {
		(*xs)[i] = xpointsValue[static_cast<size_t>(i)];
		(*ys)[i] = ypointsValue[static_cast<size_t>(i)];
	}
	xpoints = std::move(xs);
	ypoints = std::move(ys);
	calculatePath();
}

runtime::Ref<Polygon2D> Polygon2D::create(std::span<const float> xpointsValue, std::span<const float> ypointsValue, int32_t npointsValue) {
	return runtime::makeRef<Polygon2D>(xpointsValue, ypointsValue, npointsValue);
}

Polygon2D::Polygon2D(std::span<const int32_t> xpointsValue, std::span<const int32_t> ypointsValue, int32_t npointsValue) {
	if (npointsValue > static_cast<int32_t>(xpointsValue.size()) || npointsValue > static_cast<int32_t>(ypointsValue.size()))
		throw runtime::IndexOutOfBoundsException("npoints > xpoints.length || npoints > ypoints.length");
	npoints = npointsValue;
	runtime::Ref<runtime::Array<float>> xs = runtime::Array<float>::make(npointsValue);
	runtime::Ref<runtime::Array<float>> ys = runtime::Array<float>::make(npointsValue);
	for (int32_t i = 0; i < npointsValue; i++) {
		(*xs)[i] = static_cast<float>(xpointsValue[static_cast<size_t>(i)]);
		(*ys)[i] = static_cast<float>(ypointsValue[static_cast<size_t>(i)]);
	}
	xpoints = std::move(xs);
	ypoints = std::move(ys);
	calculatePath();
}

runtime::Ref<Polygon2D> Polygon2D::create(std::span<const int32_t> xpointsValue, std::span<const int32_t> ypointsValue, int32_t npointsValue) {
	return runtime::makeRef<Polygon2D>(xpointsValue, ypointsValue, npointsValue);
}

void Polygon2D::reset() {
	npoints = 0;
	bounds = Rectangle2D{};
	path = newPathOfPoints(0, false); // Java: new GeneralPath() (empty, WIND_NON_ZERO)
	closedPath = Path2D{};
}

runtime::Ref<Polygon2D> Polygon2D::clone() {
	runtime::Ref<Polygon2D> pol = create();
	runtime::Ptr<runtime::Array<float>> xs = xpoints.get();
	runtime::Ptr<runtime::Array<float>> ys = ypoints.get();
	for (int32_t i = 0, n = npoints.get(); i < n; i++)
		pol->addPoint((*xs)[i].get(), (*ys)[i].get());
	return pol;
}

void Polygon2D::calculatePath() {
	// Java: path = new GeneralPath(); path.moveTo(xpoints[0], ypoints[0]); lineTo for the other points; bounds = path.getBounds2D()
	runtime::Ptr<runtime::Array<float>> xs = xpoints.get();
	runtime::Ptr<runtime::Array<float>> ys = ypoints.get();
	static_cast<void>((*xs)[0].get()); // moveTo(xpoints[0], ypoints[0]) throws ArrayIndexOutOfBoundsException for an empty array
	static_cast<void>((*ys)[0].get());
	Path2D newPath = newPathOfPoints(std::max(npoints.get(), 1), false);
	path = newPath;
	bounds = boundsOfPath(newPath);
	closedPath = Path2D{};
}

void Polygon2D::updatePath(float x, float y) {
	closedPath = Path2D{};
	Path2D current = path.get();
	if (!current.present) {
		path = newPathOfPoints(1, true); // Java: new GeneralPath(GeneralPath.WIND_EVEN_ODD); path.moveTo(x, y)
		bounds = Rectangle2D{x, y, 0, 0, true};    // Java: new Rectangle2D.Float(x, y, 0, 0)
	} else {
		if (current.numPoints == 0)
			throwMissingMoveTo(); // Java: path.lineTo on the empty path reset() created
		current.numPoints++;
		path = current;
		Rectangle2D oldBounds = bounds.get();
		if (!oldBounds.present)
			throw runtime::NullPointerException("Polygon2D.bounds is null");
		float xmax = static_cast<float>(oldBounds.getMaxX());
		float ymax = static_cast<float>(oldBounds.getMaxY());
		float xmin = static_cast<float>(oldBounds.getMinX());
		float ymin = static_cast<float>(oldBounds.getMinY());
		if (x < xmin)
			xmin = x;
		else if (x > xmax)
			xmax = x;
		if (y < ymin)
			ymin = y;
		else if (y > ymax)
			ymax = y;
		bounds = Rectangle2D{xmin, ymin, xmax - xmin, ymax - ymin, true}; // Java: new Rectangle2D.Float (float widths)
	}
}

void Polygon2D::addPoint(float x, float y) {
	int32_t n = npoints.get();
	runtime::Ref<runtime::Array<float>> xs = xpoints.get();
	runtime::Ref<runtime::Array<float>> ys = ypoints.get();
	if (n == xs->length()) {
		runtime::Ref<runtime::Array<float>> tmp = runtime::Array<float>::make(n * 2);
		for (int32_t i = 0; i < n; i++)
			(*tmp)[i] = (*xs)[i].get();
		xpoints = tmp;
		xs = std::move(tmp);
		tmp = runtime::Array<float>::make(n * 2);
		for (int32_t i = 0; i < n; i++)
			(*tmp)[i] = (*ys)[i].get();
		ypoints = tmp;
		ys = std::move(tmp);
	}
	(*xs)[n] = x;
	(*ys)[n] = y;
	npoints = n + 1;
	updatePath(x, y);
}

bool Polygon2D::contains(int32_t x, int32_t y) {
	return contains(static_cast<double>(x), static_cast<double>(y));
}

bool Polygon2D::contains(double x, double y) {
	if (npoints.get() <= 2)
		return false;
	Rectangle2D currentBounds = bounds.get();
	if (!currentBounds.present)
		throw runtime::NullPointerException("Polygon2D.bounds is null");
	if (!currentBounds.contains(x, y))
		return false;
	updateComputingPath();
	// Java: Path2D.contains(double, double)
	Path2D closed = closedPath.get();
	if (x * 0.0 + y * 0.0 != 0.0)
		return false; // x or y is infinite or NaN
	if (closed.numPoints + (closed.closed ? 1 : 0) < 2)
		return false;
	int32_t mask = closed.windEvenOdd ? 1 : -1;
	return (pointCrossings(closed, x, y) & mask) != 0;
}

void Polygon2D::updateComputingPath() {
	if (npoints.get() >= 1) {
		if (!closedPath.get().present) {
			Path2D current = path.get();
			if (!current.present)
				throw runtime::NullPointerException("Polygon2D.path is null");
			if (current.numPoints == 0)
				throwMissingMoveTo(); // Java: closePath() on an empty path
			current.closed = true;
			closedPath = current;
		}
	}
}

bool Polygon2D::intersects(double x, double y, double w, double h) {
	if (npoints.get() <= 0)
		return false;
	Rectangle2D currentBounds = bounds.get();
	if (!currentBounds.present)
		throw runtime::NullPointerException("Polygon2D.bounds is null");
	if (!currentBounds.intersects(x, y, w, h))
		return false;
	updateComputingPath();
	// Java: Path2D.intersects(double, double, double, double)
	Path2D closed = closedPath.get();
	if (std::isnan(x + w) || std::isnan(y + h))
		return false;
	if (w <= 0 || h <= 0)
		return false;
	int32_t mask = closed.windEvenOdd ? 2 : -1;
	int32_t crossings = rectCrossings(closed, x, y, x + w, y + h);
	return crossings == RECT_INTERSECTS || (crossings & mask) != 0;
}

bool Polygon2D::intersects(const Rectangle2D& r) {
	if (!r.present)
		throw runtime::NullPointerException("Rectangle2D is null");
	return intersects(r.getX(), r.getY(), r.getWidth(), r.getHeight());
}

bool Polygon2D::contains(double x, double y, double w, double h) {
	if (npoints.get() <= 0)
		return false;
	Rectangle2D currentBounds = bounds.get();
	if (!currentBounds.present)
		throw runtime::NullPointerException("Polygon2D.bounds is null");
	if (!currentBounds.intersects(x, y, w, h))
		return false;
	updateComputingPath();
	// Java: Path2D.contains(double, double, double, double)
	Path2D closed = closedPath.get();
	if (std::isnan(x + w) || std::isnan(y + h))
		return false;
	if (w <= 0 || h <= 0)
		return false;
	int32_t mask = closed.windEvenOdd ? 2 : -1;
	int32_t crossings = rectCrossings(closed, x, y, x + w, y + h);
	return crossings != RECT_INTERSECTS && (crossings & mask) != 0;
}

bool Polygon2D::contains(const Rectangle2D& r) {
	if (!r.present)
		throw runtime::NullPointerException("Rectangle2D is null");
	return contains(r.getX(), r.getY(), r.getWidth(), r.getHeight());
}

Polygon2D::Path2D Polygon2D::newPathOfPoints(int32_t numPoints, bool windEvenOdd) noexcept {
	return Path2D{numPoints, windEvenOdd, false, true};
}

Polygon2D::Rectangle2D Polygon2D::boundsOfPath(const Path2D& pathValue) const {
	// Java 19+: Path2D.getBounds2D(PathIterator) - the extremes of the float coordinates, widened to double (Rectangle2D.Double)
	if (pathValue.numPoints <= 0)
		return Rectangle2D{0, 0, 0, 0, true};
	runtime::Ptr<runtime::Array<float>> xs = xpoints.get();
	runtime::Ptr<runtime::Array<float>> ys = ypoints.get();
	double leftX = (*xs)[0].get();
	double rightX = leftX;
	double topY = (*ys)[0].get();
	double bottomY = topY;
	for (int32_t i = 1; i < pathValue.numPoints; i++) {
		double endX = (*xs)[i].get();
		double endY = (*ys)[i].get();
		if (endX < leftX)
			leftX = endX;
		if (endX > rightX)
			rightX = endX;
		if (endY < topY)
			topY = endY;
		if (endY > bottomY)
			bottomY = endY;
	}
	return Rectangle2D{leftX, topY, rightX - leftX, bottomY - topY, true};
}

int32_t Polygon2D::pointCrossings(const Path2D& pathValue, double px, double py) const {
	if (pathValue.numPoints <= 0)
		return 0;
	runtime::Ptr<runtime::Array<float>> xs = xpoints.get();
	runtime::Ptr<runtime::Array<float>> ys = ypoints.get();
	double movx = (*xs)[0].get();
	double movy = (*ys)[0].get();
	double curx = movx;
	double cury = movy;
	int32_t crossings = 0;
	for (int32_t i = 1; i < pathValue.numPoints; i++) { // SEG_LINETO
		double endx = (*xs)[i].get();
		double endy = (*ys)[i].get();
		crossings += pointCrossingsForLine(px, py, curx, cury, endx, endy);
		curx = endx;
		cury = endy;
	}
	if (pathValue.closed) { // SEG_CLOSE
		if (cury != movy)
			crossings += pointCrossingsForLine(px, py, curx, cury, movx, movy);
		curx = movx;
		cury = movy;
	}
	if (cury != movy)
		crossings += pointCrossingsForLine(px, py, curx, cury, movx, movy);
	return crossings;
}

int32_t Polygon2D::rectCrossings(const Path2D& pathValue, double rxmin, double rymin, double rxmax, double rymax) const {
	if (pathValue.numPoints <= 0)
		return 0;
	runtime::Ptr<runtime::Array<float>> xs = xpoints.get();
	runtime::Ptr<runtime::Array<float>> ys = ypoints.get();
	double movx = (*xs)[0].get();
	double movy = (*ys)[0].get();
	double curx = movx;
	double cury = movy;
	int32_t crossings = 0;
	for (int32_t i = 1; crossings != RECT_INTERSECTS && i < pathValue.numPoints; i++) { // SEG_LINETO
		double endx = (*xs)[i].get();
		double endy = (*ys)[i].get();
		crossings = rectCrossingsForLine(crossings, rxmin, rymin, rxmax, rymax, curx, cury, endx, endy);
		curx = endx;
		cury = endy;
	}
	if (pathValue.closed && crossings != RECT_INTERSECTS) { // SEG_CLOSE
		if (curx != movx || cury != movy)
			crossings = rectCrossingsForLine(crossings, rxmin, rymin, rxmax, rymax, curx, cury, movx, movy);
		curx = movx;
		cury = movy;
	}
	if (crossings != RECT_INTERSECTS && (curx != movx || cury != movy))
		crossings = rectCrossingsForLine(crossings, rxmin, rymin, rxmax, rymax, curx, cury, movx, movy);
	return crossings;
}

Polygon2D::~Polygon2D() = default;

} // namespace aion::gameserver::model::geometry
