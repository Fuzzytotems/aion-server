#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

/**
 * Small helpers shared by the DAO bodies (P4-14): Java enum name()/valueOf() for the enum columns, and java.sql.Timestamp conversions. No Java
 * counterpart.
 */
namespace aion::gameserver::dao::detail {

/** Java enum name() / toString() of a generated enum without a toString override (the text stored in the enum columns) */
template <class E>
std::string enumName(E value) {
	return std::string(xml::enumName(value));
}

/**
 * Java E.valueOf(name) of a column value.
 *
 * @param javaEnumName the Java binary name (exception message)
 * @throws IllegalArgumentException for an unknown constant, like Java
 */
template <class E>
E enumValueOf(std::string_view name, std::string_view javaEnumName) {
	return model::templates::detail::enumValueOf<E>(name, javaEnumName);
}

/** Java new Timestamp(millis) */
inline commons::database::Timestamp toTimestamp(int64_t millis) {
	return commons::database::Timestamp{std::chrono::milliseconds(millis)};
}

/** Java timestamp.getTime() */
inline int64_t getTime(commons::database::Timestamp timestamp) {
	return timestamp.time_since_epoch().count();
}

/** Java rs.getTimestamp(column).getTime() on a column that may be NULL: NullPointerException like Java */
inline int64_t getTime(const std::optional<commons::database::Timestamp>& timestamp) {
	if (!timestamp)
		throw runtime::NullPointerException("Cannot invoke \"java.sql.Timestamp.getTime()\" because the timestamp is null");
	return getTime(*timestamp);
}

/**
 * Java text.split(String.valueOf(separator)) for a separator that is no regex metacharacter: the tokens between the separators, trailing
 * empty tokens removed; a text without the separator (the empty text included) is the only token.
 */
inline std::vector<std::string_view> javaSplit(std::string_view text, char separator) {
	std::vector<std::string_view> tokens;
	size_t start = 0;
	while (true) {
		const size_t found = text.find(separator, start);
		if (found == std::string_view::npos) {
			tokens.push_back(text.substr(start));
			break;
		}
		tokens.push_back(text.substr(start, found - start));
		start = found + 1;
	}
	if (tokens.size() > 1) {
		while (!tokens.empty() && tokens.back().empty())
			tokens.pop_back();
	}
	return tokens;
}

/** Java setBytes(index, byte[]) of a nullable byte array: std::nullopt for null */
inline std::optional<std::vector<uint8_t>> toBytes(const runtime::Ptr<runtime::Array<int8_t>>& array) {
	if (!array)
		return std::nullopt;
	std::vector<uint8_t> bytes;
	bytes.reserve(static_cast<size_t>(array->length()));
	for (int8_t value : *array)
		bytes.push_back(static_cast<uint8_t>(value));
	return bytes;
}

/** Java rs.getBytes(column) as a runtime byte array: null for SQL NULL */
inline runtime::Ref<runtime::Array<int8_t>> toArray(const std::optional<std::vector<uint8_t>>& bytes) {
	if (!bytes)
		return nullptr;
	runtime::Ref<runtime::Array<int8_t>> array = runtime::Array<int8_t>::make(static_cast<int32_t>(bytes->size()));
	for (size_t i = 0; i < bytes->size(); ++i)
		(*array)[static_cast<int32_t>(i)].set(static_cast<int8_t>((*bytes)[i]));
	return array;
}

} // namespace aion::gameserver::dao::detail
