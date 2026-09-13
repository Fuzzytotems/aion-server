#pragma once

#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/commons/configuration/transformers/CommaSeparatedValueTransformer.h"
#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

/**
 * Creates a std::vector containing the comma separated items (see CommaSeparatedValueTransformer::splitAndTrimValues), each transformed by the
 * transformer of T (so an invalid item fails with a nested TransformationException naming the item). An empty value yields an empty vector.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.ArrayTransformer (arrays) and CollectionTransformer (List as ArrayList)
 *
 * @author Neon
 */
template <Transformable T, typename Allocator>
struct PropertyTransformer<std::vector<T, Allocator>> {
	static std::string typeName() { return transformers::typeName<T>() + "[]"; }

	static std::vector<T, Allocator> parseObject(std::string_view value) {
		std::vector<std::string> values = CommaSeparatedValueTransformer::splitAndTrimValues(value);
		std::vector<T, Allocator> result;
		result.reserve(values.size());
		for (const std::string& item : values)
			result.push_back(transform<T>(item));
		return result;
	}
};

/**
 * Creates a std::set containing the comma separated items (duplicates are dropped). An empty value yields an empty set.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.CollectionTransformer (Set as HashSet; iteration order of a std::set is sorted instead)
 *
 * @author Neon
 */
template <Transformable T, typename Compare, typename Allocator>
struct PropertyTransformer<std::set<T, Compare, Allocator>> {
	static std::string typeName() { return "Set"; }

	static std::set<T, Compare, Allocator> parseObject(std::string_view value) {
		std::set<T, Compare, Allocator> result;
		for (const std::string& item : CommaSeparatedValueTransformer::splitAndTrimValues(value))
			result.insert(transform<T>(item));
		return result;
	}
};

/**
 * Creates a std::unordered_set containing the comma separated items (duplicates are dropped). An empty value yields an empty set.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.CollectionTransformer (Set as HashSet)
 *
 * @author Neon
 */
template <Transformable T, typename Hash, typename KeyEqual, typename Allocator>
struct PropertyTransformer<std::unordered_set<T, Hash, KeyEqual, Allocator>> {
	static std::string typeName() { return "Set"; }

	static std::unordered_set<T, Hash, KeyEqual, Allocator> parseObject(std::string_view value) {
		std::unordered_set<T, Hash, KeyEqual, Allocator> result;
		for (const std::string& item : CommaSeparatedValueTransformer::splitAndTrimValues(value))
			result.insert(transform<T>(item));
		return result;
	}
};

} // namespace aion::commons::configuration::transformers
