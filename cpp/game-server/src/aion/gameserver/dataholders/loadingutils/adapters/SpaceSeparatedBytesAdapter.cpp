#include "aion/gameserver/dataholders/loadingutils/adapters/SpaceSeparatedBytesAdapter.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"

namespace aion::gameserver::xml::adapters {

std::vector<int8_t> parseSpaceSeparatedBytes(std::string_view value) {
	std::vector<std::string> parts = commons::utils::StringUtils::splitJava(value, " ");
	std::vector<int8_t> bytes;
	bytes.reserve(parts.size());
	for (const std::string& part : parts) {
		int32_t parsed = 0;
		try {
			parsed = commons::utils::parseInt(part);
		} catch (const commons::utils::NumberFormatException& e) {
			throw XmlValueException(std::string(e.what()) + " (in space separated bytes '" + std::string(value) + "')");
		}
		if (parsed < -128 || parsed > 127)
			throw XmlValueException("Value out of range. Value:\"" + part + "\" Radix:10 (in space separated bytes '" + std::string(value) + "')");
		bytes.push_back(static_cast<int8_t>(parsed));
	}
	return bytes;
}

std::vector<int8_t> parseSpaceSeparatedBytes(BindContext& context, std::string_view value) {
	return context.adapt([](std::string_view v) { return parseSpaceSeparatedBytes(v); }, value);
}

std::string printSpaceSeparatedBytes(std::span<const int8_t> bytes) {
	std::string result;
	result.reserve(bytes.size() * 3);
	for (size_t i = 0; i < bytes.size(); ++i) {
		if (i > 0)
			result += ' ';
		result += std::to_string(bytes[i]);
	}
	return result;
}

} // namespace aion::gameserver::xml::adapters
