#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include <magic_enum/magic_enum.hpp>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"
#include "aion/commons/utils/Exception.h"

namespace aion::commons::configuration::transformers {

/**
 * Transforms enum string representation to enum. String must match case definition of enum, for instance:
 * <pre>
 * enum class Mode { FILE, URL };
 * </pre>
 * will be parsed with string "FILE" but not "file".
 * <p>
 * Java returns null for an empty value. Bind a std::optional&lt;E&gt; for such nullable fields (empty value = std::nullopt, see
 * OptionalTransformer.h). Deviation: for a plain enum field, an empty value is an error, since there is no null to assign.
 * <p>
 * Names are looked up with magic_enum, which only sees enumerators whose values are within [MAGIC_ENUM_RANGE_MIN, MAGIC_ENUM_RANGE_MAX]
 * (default -128..127). Java-style enums with consecutive values starting at 0 are fine.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.EnumTransformer
 *
 * @author SoulKeeper
 */
template <typename E>
  requires std::is_enum_v<E>
struct PropertyTransformer<E> {
	static std::string typeName() { return std::string(magic_enum::enum_type_name<E>()); }

	static E parseObject(std::string_view value) {
		if (value.empty())
			throw utils::IllegalArgumentException("Cannot convert empty string to enum " + typeName() + " (bind a std::optional to allow empty values)");
		if (auto result = magic_enum::enum_cast<E>(value))
			return *result;
		// Java: Enum.valueOf
		throw utils::IllegalArgumentException("No enum constant " + typeName() + "." + std::string(value));
	}
};

} // namespace aion::commons::configuration::transformers
