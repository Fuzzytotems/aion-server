#pragma once

#include <concepts>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>

#include "aion/commons/configuration/TransformationException.h"

namespace aion::commons::configuration::transformers {

/**
 * Converts a property value (UTF-8 string) to a field value of type T. This primary template is intentionally left undefined: every supported
 * type has a (partial) specialization, so binding a field of an unsupported type fails at compile time (Java: "Transformer for X is not
 * registered." at runtime).
 * <p>
 * <b>Adding a type</b> (Java: PropertyTransformers.register(new XTransformer())): specialize this template in this namespace, before the binding
 * code that uses the type is compiled (i.e. include the header with the specialization in the config .cpp file):
 * <pre>
 * namespace aion::commons::configuration::transformers {
 * template &lt;&gt;
 * struct PropertyTransformer&lt;gameserver::CronExpression&gt; {
 *   // optional, used in error messages (Java: Class.getSimpleName()); defaults to the C++ type name
 *   static std::string typeName() { return "CronExpression"; }
 *   // Java: parseObject(String, TransformationTypeInfo). Throw any std::exception on invalid input, it becomes the cause of the
 *   // TransformationException thrown by transform().
 *   static gameserver::CronExpression parseObject(std::string_view value);
 * };
 * }
 * </pre>
 * Containers (std::vector, std::set, std::unordered_set, maps for ConfigurableProcessor::bindPattern) and std::optional then work with the new
 * element type automatically.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.PropertyTransformer (plus TransformationTypeInfo, whose job is done by template
 * arguments)
 *
 * @author SoulKeeper, Neon
 */
template <typename T>
struct PropertyTransformer;

/** Satisfied if a PropertyTransformer specialization for T is visible. */
template <typename T>
concept Transformable = requires(std::string_view value) {
	{ PropertyTransformer<T>::parseObject(value) } -> std::convertible_to<T>;
};

namespace detail {

/** The C++ type name without MSVC's "class "/"struct "/"enum " prefixes. */
std::string cppTypeName(const std::type_info& type);

template <typename T>
concept HasTypeName = requires {
	{ PropertyTransformer<T>::typeName() } -> std::convertible_to<std::string_view>;
};

} // namespace detail

/** @return the type name used in error messages (Java: Class.getSimpleName()) */
template <Transformable T>
std::string typeName() {
	if constexpr (detail::HasTypeName<T>)
		return std::string(PropertyTransformer<T>::typeName());
	else
		return detail::cppTypeName(typeid(T));
}

/**
 * Transforms a value to an object of type T.
 * <p>
 * Java: PropertyTransformer.transform(String, ...)
 *
 * @throws TransformationException "Error parsing \"&lt;value&gt;\" as &lt;type name&gt;", with the parse error as cause
 */
template <Transformable T>
T transform(std::string_view value) {
	try {
		return PropertyTransformer<T>::parseObject(value);
	} catch (...) {
		throw TransformationException("Error parsing \"" + std::string(value) + "\" as " + typeName<T>(), std::current_exception());
	}
}

} // namespace aion::commons::configuration::transformers
