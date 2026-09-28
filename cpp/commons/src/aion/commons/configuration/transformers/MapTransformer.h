#pragma once

#include <exception>
#include <ranges>
#include <string>
#include <utility>

#include "aion/commons/configuration/TransformationException.h"
#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

/** A map (std::map, std::unordered_map, ...) whose key and value types have transformers. */
template <typename Map>
concept TransformableMap =
  requires {
	  typename Map::key_type;
	  typename Map::mapped_type;
  } && Transformable<typename Map::key_type> && Transformable<typename Map::mapped_type> &&
  requires(Map& map, typename Map::key_type key, typename Map::mapped_type value) { map.insert_or_assign(std::move(key), std::move(value)); };

/**
 * Creates the maps of ConfigurableProcessor::bindPattern (Java: @Properties fields).
 * <p>
 * Java picks an EnumMap for enum keys (iterated in ordinal order) and a HashMap otherwise. In C++ the field type decides: a std::map with enum
 * keys iterates in the order of the enumerator values, which equals Java's ordinal order for enums with consecutive values starting at 0.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.MapTransformer
 */
namespace MapTransformer {

/**
 * Transforms each key and value with the transformers of the map's key and value type. Later entries replace earlier ones with an equal key.
 *
 * @param values input range of (key, value) string pairs, e.g. a std::map&lt;std::string, std::string&gt;
 * @throws TransformationException if a key cannot be transformed ("Error parsing ...") or if a value cannot be transformed ("Could not transform
 *           property: &lt;key&gt;", caused by the "Error parsing ..." exception)
 */
template <TransformableMap Map, std::ranges::input_range Values>
Map transform(const Values& values) {
	Map output;
	for (const auto& [k, v] : values) {
		auto key = transformers::transform<typename Map::key_type>(k);
		try {
			output.insert_or_assign(std::move(key), transformers::transform<typename Map::mapped_type>(v));
		} catch (...) {
			throw TransformationException("Could not transform property: " + std::string(k), std::current_exception());
		}
	}
	return output;
}

} // namespace MapTransformer

} // namespace aion::commons::configuration::transformers
