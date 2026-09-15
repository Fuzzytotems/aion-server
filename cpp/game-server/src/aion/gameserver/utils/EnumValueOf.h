#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::utils {

/**
 * Java: the IllegalArgumentException that Enum.valueOf throws for an unknown constant name, with Java's message
 * "No enum constant com.aionemu.gameserver.model.siege.SiegeRace.invalidName" (handlers-and-porting-plan.md §1.10).
 * <p>
 * ChatCommand.toErrorMessage parses the Java message and looks the enum class up with Class.forName to list its constants; the C++ port checks
 * for this type with dynamic_cast and reads the simple name and the constants from it instead.
 */
class EnumConstantException : public runtime::IllegalArgumentException {
public:
	EnumConstantException(std::string enumCanonicalName, std::string enumSimpleName, std::string value, std::vector<std::string> allValues);

	/** Java: Class.getCanonicalName() of the enum, e.g. "com.aionemu.gameserver.model.siege.SiegeRace" */
	const std::string& getEnumCanonicalName() const noexcept { return enumCanonicalName; }

	/** Java: Class.getSimpleName() of the enum, e.g. "SiegeRace" */
	const std::string& getEnumSimpleName() const noexcept { return enumSimpleName; }

	/** The name that matched no constant */
	const std::string& getValue() const noexcept { return value; }

	/** Java: Enum.toString() of every constant in ordinal order (the constant names) */
	const std::vector<std::string>& getAllValues() const noexcept { return allValues; }

private:
	std::string enumCanonicalName;
	std::string enumSimpleName;
	std::string value;
	std::vector<std::string> allValues;
};

namespace detail {

/**
 * Java: Class.getCanonicalName() of a generated game server enum: the C++ namespace mapped back to the Java package ("aion::gameserver::" ->
 * "com.aionemu.gameserver.", keyword segments without their trailing '_') and a hoisted nested enum "Outer_Inner" (javaName "Inner") spelled
 * "Outer.Inner".
 */
std::string enumCanonicalName(const std::type_info& type, std::string_view javaName);

[[noreturn]] void throwEnumConstantException(const std::type_info& type, std::string_view javaName, std::string_view value,
	std::span<const std::string_view> names);

} // namespace detail

/**
 * Java: Enum.valueOf(E.class, name) / E.valueOf(name) - the constant with exactly this name.
 *
 * @throws EnumConstantException
 *           (an IllegalArgumentException) if the enum has no constant with the name
 */
template <::aion::gameserver::xml::XmlEnum E>
E enumValueOf(std::string_view name) {
	if (std::optional<E> value = ::aion::gameserver::xml::enumFromName<E>(name))
		return *value;
	using Traits = ::aion::gameserver::xml::EnumTraits<E>;
	detail::throwEnumConstantException(typeid(E), Traits::javaName, name, Traits::names);
}

} // namespace aion::gameserver::utils
