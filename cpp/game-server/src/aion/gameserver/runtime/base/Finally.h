#pragma once

#include <exception>
#include <type_traits>
#include <utility>

namespace aion::gameserver::runtime {

/**
 * Scope guard for Java `finally` blocks and cycle breakers that must run even when the body throws (design §5.3 LogoutBreakers, R12):
 * `auto breakers = finally([&] { LogoutBreakers::run(player); });`
 *
 * The action runs exactly once when the guard is destroyed (normal exit or unwinding). An exception escaping the action is swallowed by the
 * destructor (throwing during unwinding would terminate), so actions that can fail must catch and log inside (design: "noexcept, each step
 * logged on exception").
 */
template <class F>
	requires std::is_nothrow_move_constructible_v<F>
class [[nodiscard]] FinallyGuard {
public:
	explicit FinallyGuard(F action) noexcept : action(std::move(action)) {}
	FinallyGuard(FinallyGuard&& other) noexcept : action(std::move(other.action)), active(std::exchange(other.active, false)) {}
	FinallyGuard(const FinallyGuard&) = delete;
	FinallyGuard& operator=(const FinallyGuard&) = delete;
	FinallyGuard& operator=(FinallyGuard&&) = delete;

	~FinallyGuard() {
		if (active) {
			try {
				action();
			} catch (...) {
				// actions are required to handle their own errors; swallowing keeps the unwinding exception intact
			}
		}
	}

	/** Disarms the guard (the action will not run). */
	void dismiss() noexcept { active = false; }

private:
	F action;
	bool active = true;
};

template <class F>
FinallyGuard<std::decay_t<F>> finally(F&& action) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F&&>) {
	return FinallyGuard<std::decay_t<F>>(std::forward<F>(action));
}

} // namespace aion::gameserver::runtime
