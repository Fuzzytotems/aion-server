#include "aion/gameserver/utils/EnumValueOf.h"

#include <utility>

#include "aion/commons/utils/ClassName.h"

namespace aion::gameserver::utils {

EnumConstantException::EnumConstantException(std::string enumCanonicalNameValue, std::string enumSimpleNameValue, std::string valueArg,
	std::vector<std::string> allValuesValue)
	: IllegalArgumentException("No enum constant " + enumCanonicalNameValue + "." + valueArg), enumCanonicalName(std::move(enumCanonicalNameValue)),
	  enumSimpleName(std::move(enumSimpleNameValue)), value(std::move(valueArg)), allValues(std::move(allValuesValue)) {
}

namespace detail {

std::string enumCanonicalName(const std::type_info& type, std::string_view javaName) {
	std::string cppName = commons::utils::getClassName(type); // "aion::gameserver::model::siege::SiegeRace"
	std::vector<std::string> segments;
	for (size_t start = 0;;) {
		size_t colons = cppName.find("::", start);
		segments.push_back(cppName.substr(start, colons == std::string::npos ? std::string::npos : colons - start));
		if (colons == std::string::npos)
			break;
		start = colons + 2;
	}
	std::string result;
	size_t first = 0;
	if (segments.size() > 2 && segments[0] == "aion" && segments[1] == "gameserver") {
		result = "com.aionemu.gameserver";
		first = 2;
	}
	for (size_t i = first; i < segments.size(); ++i) {
		std::string segment = segments[i];
		if (i + 1 == segments.size()) {
			// a hoisted nested enum Outer_Inner whose Java simple name is Inner
			if (segment.size() > javaName.size() + 1 && segment.ends_with(javaName) && segment[segment.size() - javaName.size() - 1] == '_')
				segment.replace(segment.size() - javaName.size() - 1, 1, ".");
		} else if (segment.size() > 1 && segment.back() == '_') {
			segment.pop_back(); // keyword namespace segment, e.g. template_ (CONVENTIONS.md keyword rule)
		}
		if (!result.empty())
			result += '.';
		result += segment;
	}
	return result;
}

void throwEnumConstantException(const std::type_info& type, std::string_view javaName, std::string_view value,
	std::span<const std::string_view> names) {
	std::vector<std::string> allValues;
	allValues.reserve(names.size());
	for (std::string_view name : names)
		allValues.emplace_back(name);
	throw EnumConstantException(enumCanonicalName(type, javaName), std::string(javaName), std::string(value), std::move(allValues));
}

} // namespace detail

} // namespace aion::gameserver::utils
