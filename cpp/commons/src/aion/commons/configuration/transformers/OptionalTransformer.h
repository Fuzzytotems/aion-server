#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

/**
 * Nullable fields: an empty value yields std::nullopt, any other value is parsed by the transformer of T.
 * <p>
 * This is the C++ counterpart of Java transformers that return null for an empty value: EnumTransformer (std::optional&lt;E&gt;), PatternTransformer
 * (std::optional&lt;std::regex&gt;) and the game server's CronExpressionTransformer. Note that Java has no such null semantics for other types (e.g.
 * an empty Integer value is a parse error there), so use std::optional only where the Java field can be null.
 * <p>
 * Not in Java.
 */
template <Transformable T>
struct PropertyTransformer<std::optional<T>> {
	static std::string typeName() { return transformers::typeName<T>(); }

	static std::optional<T> parseObject(std::string_view value) {
		if (value.empty())
			return std::nullopt;
		return PropertyTransformer<T>::parseObject(value);
	}
};

} // namespace aion::commons::configuration::transformers
