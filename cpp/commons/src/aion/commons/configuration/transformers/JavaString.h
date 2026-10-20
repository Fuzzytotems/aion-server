#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

namespace aion::commons::configuration::transformers {

/**
 * The value formatter of commons' named binds (ConfigurableProcessor::bindNamed): Java's String.valueOf of the commons config fields' types
 * (strings, integers, booleans and an optional integer, "null" when empty). The game server's config classes use their own formatter, which
 * also knows floats (Java's Float.toString), enums, collections and the game server's types (configs/detail/JavaString.h).
 * <p>
 * Not in Java.
 */
struct BasicJavaString {
	static std::string format(const std::string& value) { return value; }

	static std::string format(bool value) { return value ? "true" : "false"; }

	template <class T>
		requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
	static std::string format(T value) {
		return std::to_string(static_cast<int64_t>(value));
	}

	template <class T>
	static std::string format(const std::optional<T>& value) {
		return value.has_value() ? format(*value) : std::string("null");
	}
};

} // namespace aion::commons::configuration::transformers
