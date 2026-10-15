#pragma once

#include <source_location>
#include <string_view>

/**
 * Checked builds (design §12.5).
 *
 * AION_CHECKED is 1 in Debug and RelWithDebInfo and 0 in Release (set by game-server/CMakeLists.txt, AION_CHECKED_MODE). Checked builds add
 * the runtime checks C1-C16 (Ptr scope stamps, TaskScope assertions, cookies, lock-order validation, destructor context, ...). Release builds
 * keep C5 (Reclaimer invariants), the leaf lock ranks of C6 and the watchdog (C7), see C14.
 *
 * Failure kinds used by the kernel:
 * - exceptions (IllegalStateException etc.): checks whose failure is a porting bug that the calling code may report and survive (C1, C6, C9, C10)
 * - AION_CHECK / AION_CHECK_ALWAYS: fatal invariant violations (C2, C3, C4, C5, C8, C11, C12, C15). They log the message with a stack trace
 *   to stderr and the "com.aionemu.gameserver.runtime" logger (best effort), then terminate the process with std::abort (death tests catch this).
 */
#if !defined(AION_CHECKED)
#if defined(NDEBUG)
#define AION_CHECKED 0
#else
#define AION_CHECKED 1
#endif
#endif

namespace aion::gameserver::runtime {

/** true in checked builds, for `if (CHECKED)` in code that must compile in both modes */
inline constexpr bool CHECKED = AION_CHECKED != 0;

/** Details of a failed fatal check, passed to the check failure handler before the process terminates. */
struct CheckFailure {
	/** check id from the design, e.g. "C4", or a short name of the failed condition */
	std::string_view check;
	/** human-readable description including the class name where the design requires it ("terminate with class") */
	std::string_view message;
	std::source_location where;
};

/**
 * Handler invoked by checkFailed before terminating. The default handler prints the failure and the stack trace to stderr and logs it.
 * Tests may install a handler that records the failure (it must not return normally if it wants to avoid termination: it may throw only if
 * the failing check is known to be outside a noexcept function). Thread-safe (atomic).
 */
using CheckFailureHandler = void (*)(const CheckFailure& failure);

/** Installs a handler; nullptr restores the default. Returns the previous handler. */
CheckFailureHandler setCheckFailureHandler(CheckFailureHandler handler) noexcept;

/**
 * Reports a fatal check failure: calls the installed handler, then terminates (std::abort). Never returns. noexcept: usable from noexcept
 * kernel paths (retain/release, destructors).
 */
[[noreturn]] void checkFailed(std::string_view check, std::string_view message, std::source_location where = std::source_location::current()) noexcept;

} // namespace aion::gameserver::runtime

/** Fatal check, active in checked builds only. `message` is only evaluated on failure. */
#define AION_CHECK(check_id, condition, message)                                                                                                    \
	do {                                                                                                                                              \
		if (::aion::gameserver::runtime::CHECKED && !(condition)) [[unlikely]]                                                                          \
			::aion::gameserver::runtime::checkFailed(check_id, message);                                                                                  \
	} while (false)

/** Fatal check, active in every build (C5 Reclaimer invariants, C14). `message` is only evaluated on failure. */
#define AION_CHECK_ALWAYS(check_id, condition, message)                                                                                             \
	do {                                                                                                                                              \
		if (!(condition)) [[unlikely]]                                                                                                                  \
			::aion::gameserver::runtime::checkFailed(check_id, message);                                                                                  \
	} while (false)
