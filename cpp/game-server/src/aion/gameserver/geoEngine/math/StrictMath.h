#pragma once

namespace aion::gameserver::geoEngine::math {

/**
 * Java: java.lang.StrictMath asin/acos/atan/atan2, i.e. the fdlibm 5.3 algorithms (JDK's FdLibm.java), bit for bit.
 * <p>
 * Java's Math.asin, Math.acos, Math.atan and Math.atan2 delegate to StrictMath (HotSpot has no x86_64 intrinsic for them), so these give the
 * exact results of the Java server; the C runtime's versions may differ in the last bit. FastMath.asin/acos/atan/atan2 use them.
 * <p>
 * Not in the java.lang layer of commons because only the geo math needs it so far; other ports of Math.atan2 (e.g. PositionUtil) can use it.
 */
struct StrictMath final {
	StrictMath() = delete;

	/** Java: StrictMath.asin (NaN for |x| > 1 or NaN, sign-preserving for +-0) */
	static double asin(double x) noexcept;
	/** Java: StrictMath.acos (NaN for |x| > 1 or NaN) */
	static double acos(double x) noexcept;
	/** Java: StrictMath.atan */
	static double atan(double x) noexcept;
	/** Java: StrictMath.atan2 with all IEEE special cases (signed zeros, infinities, NaN) */
	static double atan2(double y, double x) noexcept;
};

} // namespace aion::gameserver::geoEngine::math
