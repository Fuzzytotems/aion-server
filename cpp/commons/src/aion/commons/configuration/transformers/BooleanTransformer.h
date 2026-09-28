#pragma once

#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::configuration::transformers {

/**
 * This class implements basic boolean transformer.
 * <p>
 * Boolean can be represented by true / false (case doesn't matter) or 1 / 0. Everything else is an error.
 * Deviation: Java's equalsIgnoreCase uses Unicode case folding, so it would also accept e.g. "falſe" with U+017F; only ASCII case folding is
 * applied here.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.BooleanTransformer
 *
 * @author SoulKeeper
 */
template <>
struct PropertyTransformer<bool> {
	static std::string typeName() { return "boolean"; }

	static bool parseObject(std::string_view value) {
		// not using "Boolean.parseBoolean" since it never throws an error (returns false if string is not "true" ignoring case)
		if (utils::StringUtils::equalsIgnoreCase(value, "true") || value == "1")
			return true;
		if (utils::StringUtils::equalsIgnoreCase(value, "false") || value == "0")
			return false;
		throw utils::IllegalArgumentException("Only true, false, 1 and 0 are allowed.");
	}
};

} // namespace aion::commons::configuration::transformers
