#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <utility>

#include "aion/gameserver/utils/collections/fwd.h"

namespace aion::gameserver::utils::collections {

/**
 * C++: a static-only class (fieldmap K5). forEach is a member function template over any range (a collection shim, a snapshot vector) whose
 * consumer receives the range's element; an exception thrown by the consumer is logged with the element's toString() and the details, and the
 * iteration continues, like Java's `catch (Exception e)`.
 */
class CollectionUtil {
public:
	CollectionUtil() = delete;

	template <class Iterable, class Consumer>
	static void forEach(Iterable&& iterable, Consumer&& consumer) {
		forEach(std::forward<Iterable>(iterable), std::forward<Consumer>(consumer), std::function<std::string()>());
	}

	/** @param exceptionLogDetailsSupplier may be empty (Java: null) */
	template <class Iterable, class Consumer>
	static void forEach(Iterable&& iterable, Consumer&& consumer, const std::function<std::string()>& exceptionLogDetailsSupplier) {
		for (auto&& object : iterable) {
			try {
				std::invoke(consumer, object);
			} catch (...) {
				logError(describe(object), exceptionLogDetailsSupplier);
			}
		}
	}

private:
	/** Java: String.valueOf(object) for the log message: toString() of objects that have one, "null" for null references */
	template <class T>
	static std::string describe(const T& object) {
		if constexpr (requires { object == nullptr; } && requires { object->toString(); }) {
			return object == nullptr ? std::string("null") : std::string(object->toString());
		} else if constexpr (requires { object.toString(); }) {
			return std::string(const_cast<T&>(object).toString());
		} else if constexpr (std::convertible_to<const T&, std::string>) {
			return std::string(object);
		} else if constexpr (requires { std::to_string(object); }) {
			return std::to_string(object);
		} else {
			return std::string("?");
		}
	}

	/** Logs "Could not perform operation on {}{}" with the current exception (defined in the .cpp, which owns the logger) */
	static void logError(const std::string& object, const std::function<std::string()>& exceptionLogDetailsSupplier);
};

} // namespace aion::gameserver::utils::collections
