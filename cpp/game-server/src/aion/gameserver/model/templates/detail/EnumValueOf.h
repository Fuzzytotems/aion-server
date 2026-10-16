#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::model::templates::detail {

/**
 * Java `E.valueOf(name)` for a generated enum: the constant with that name.
 *
 * @param javaEnumName the Java binary name of the enum, for the exception message
 * @throws IllegalArgumentException if there is no constant with that name (Java's message)
 */
template <class E>
E enumValueOf(std::string_view name, std::string_view javaEnumName) {
	std::optional<E> constant = xml::enumFromName<E>(name);
	if (!constant)
		throw commons::utils::IllegalArgumentException("No enum constant " + std::string(javaEnumName) + "." + std::string(name));
	return *constant;
}

} // namespace aion::gameserver::model::templates::detail
