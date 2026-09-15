#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/geoEngine/collision/bih/BIHNode.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/utils/fwd.h"

namespace aion::gameserver::geoEngine::utils {

/**
 * Temporary variables assigned to each thread. Engine classes may access these temp variables with TempVars.get(), all retrieved TempVars
 * instances must be returned via TempVars.release(). This returns an available instance of the TempVar class ensuring this particular instance
 * is never used elsewhere in the meantime.
 * <p>
 * C++: a thread-confined scratch pool (Java ThreadLocal, design §3.2 `ThreadLocal<T>` → `thread_local T`). fieldmap.toml makes TempVars,
 * TempVarsStack and BIHNode.BIHStackData K5 confined (the thread-local stack is the only holder, which the escape analysis does not treat as
 * confinement; header request geo-3), so the members are plain values. Float and small integer arrays are std::array, the BIH stack a
 * std::vector of value entries.
 * The geomath classes (Ray) use plain locals instead (geoEngine/math/Ray.h); the geo engine uses TempVars at the Java call sites, so the Java
 * aliasing of the scratch vectors is kept.
 */
class TempVars {
private:
	/** Allow X instances of TempVars in a single thread. */
	static constexpr int32_t STACK_SIZE = 5;

public:
	/** Java: private static class TempVarsStack - a stack of TempVars; get() pushes, release() pops (fieldmap.toml: K5, header request geo-3) */
	struct TempVarsStack {
		int32_t index = 0;
		std::array<std::unique_ptr<TempVars>, STACK_SIZE> tempVars{};
	};

private:
	/** ThreadLocal to store a TempVarsStack for each thread. */
	static thread_local TempVarsStack varsLocal; // fieldmap.toml: the thread-local stack object itself (header request geo-3)

	/** This instance of TempVars has been retrieved but not released yet. */
	bool isUsed = false;

	TempVars() = default;

public:
	TempVars(const TempVars&) = delete;
	TempVars& operator=(const TempVars&) = delete;

	/**
	 * Acquire an instance of the TempVar class. You have to release the instance after use by calling the release() method.
	 *
	 * @throws ArrayIndexOutOfBoundsException if more than STACK_SIZE (5) instances are requested in a single thread
	 */
	static TempVars& get();

	/**
	 * Releases this instance of TempVars. The TempVars must be released in the opposite order that they are retrieved.
	 *
	 * @throws IllegalStateException if this instance was already released or a later instance was not released
	 */
	void release();

	/**
	 * C++ only: releases the instance when the scope ends while it is still in use, which only happens when an exception leaves the code
	 * between get() and release() (Java keeps the instance marked as used, so the thread's stack fills up after five such exceptions; see
	 * DEVIATIONS). Every Java release() call stays where Java has it; the guard is a no-op after it.
	 */
	class ReleaseGuard {
	public:
		explicit ReleaseGuard(TempVars& vars) noexcept : vars(&vars) {}
		~ReleaseGuard();
		ReleaseGuard(const ReleaseGuard&) = delete;
		ReleaseGuard& operator=(const ReleaseGuard&) = delete;

	private:
		TempVars* vars; // confined: a stack guard of the calling thread's own TempVars
	};

	/** General vectors. */
	math::Vector3f vect1;
	math::Vector3f vect2;
	math::Vector3f vect3;
	math::Vector3f vect4;
	math::Vector3f vect5;
	math::Vector3f vect6;
	math::Matrix3f tempMat3;
	/** BoundingBox ray collision */
	std::array<float, 3> fWdU{};
	std::array<float, 3> fAWdU{};
	std::array<float, 3> fDdU{};
	std::array<float, 3> fADdU{};
	std::array<float, 3> fAWxDdU{};
	/** BIHTree */
	std::array<int8_t, 3> bihSwapTmp{};
	std::array<int16_t, 3> bihSwapTmpShort{};
	/** empty outside BIHNode::intersectWhere, which clears it on every exit (its entries hold Ref<BIHNode>, runtime-architecture.md L15) */
	std::vector<collision::bih::BIHNode::BIHStackData> bihStack{};
};

} // namespace aion::gameserver::geoEngine::utils
