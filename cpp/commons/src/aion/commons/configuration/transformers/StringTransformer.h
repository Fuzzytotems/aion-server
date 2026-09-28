#pragma once

#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

/**
 * This class is here just for writing less "ifs" in the code. Does nothing: the value is used as is.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.StringTransformer
 *
 * @author SoulKeeper
 */
template <>
struct PropertyTransformer<std::string> {
	static std::string typeName() { return "String"; }

	static std::string parseObject(std::string_view value) { return std::string(value); }
};

} // namespace aion::commons::configuration::transformers
