#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include "aion/commons/utils/InetSocketAddress.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::configs::detail {

/**
 * The value formatter of the game server's named binds (AION_BIND, ConfigurableProcessor::bindNamed): Java's String.valueOf of a config field,
 * with Arrays.toString for arrays, as //configure's getFieldValue prints it (Configure.java:115-128).
 * <p>
 * Booleans "true"/"false"; integers in decimal; float and double Java's Float.toString / Double.toString (JavaFloat); strings as they are; enums
 * by name; an empty optional and a null pointer "null"; a path its text; an InetSocketAddress "/host:port" (Java's form of an address created
 * from a literal); a CronExpression its expression; a time zone its name; arrays, lists and sets "[a, b]"; maps "{k=v, k2=v2}".
 * <p>
 * C++ deviation: hash sets and hash maps (Java HashSet/HashMap) print their elements sorted; Java prints them in its hash order. Regular
 * expressions are not formatted here: ConfigurableProcessor prints the source text it remembered (std::regex keeps none).
 * <p>
 * Not in Java.
 */
struct JavaString {
	static std::string format(const std::string& value) { return value; }

	static std::string format(bool value) { return value ? "true" : "false"; }

	template <class T>
		requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
	static std::string format(T value) {
		return std::to_string(static_cast<int64_t>(value));
	}

	static std::string format(float value) { return geoEngine::math::JavaFloat::toString(value); }

	static std::string format(double value) { return geoEngine::math::JavaFloat::doubleToString(value); }

	template <class E>
		requires std::is_enum_v<E>
	static std::string format(E value) {
		return std::string(magic_enum::enum_name(value));
	}

	static std::string format(const std::filesystem::path& value) { return value.string(); }

	static std::string format(const commons::utils::InetSocketAddress& value) { return "/" + value.toString(); }

	static std::string format(const services::cron::CronExpression* value) { return value != nullptr ? value->toString() : "null"; }

	static std::string format(const std::chrono::time_zone* value) { return value != nullptr ? std::string(value->name()) : "null"; }

	template <class T>
	static std::string format(const std::optional<T>& value) {
		return value.has_value() ? format(*value) : std::string("null");
	}

	template <class T>
	static std::string format(const std::vector<T>& values) {
		return list(values);
	}

	template <class T, class Compare>
	static std::string format(const std::set<T, Compare>& values) {
		return list(values);
	}

	template <class T>
	static std::string format(const std::unordered_set<T>& values) {
		std::vector<T> sorted(values.begin(), values.end());
		std::ranges::sort(sorted);
		return list(sorted);
	}

	template <class K, class V, class Compare>
	static std::string format(const std::map<K, V, Compare>& values) {
		return map(values);
	}

	template <class K, class V>
	static std::string format(const std::unordered_map<K, V>& values) {
		std::vector<std::pair<K, V>> sorted(values.begin(), values.end());
		std::ranges::sort(sorted);
		return map(sorted);
	}

private:
	template <class Range>
	static std::string list(const Range& values) {
		std::string text = "[";
		for (const auto& value : values) {
			if (text.size() > 1)
				text += ", ";
			text += format(value);
		}
		return text + "]";
	}

	template <class Range>
	static std::string map(const Range& values) {
		std::string text = "{";
		for (const auto& [key, value] : values) {
			if (text.size() > 1)
				text += ", ";
			text += format(key) + "=" + format(value);
		}
		return text + "}";
	}
};

} // namespace aion::gameserver::configs::detail
