#pragma once

#include <string_view>

namespace aion::commons::configuration {

/**
 * Constants of the Java annotations that mark configuration fields. C++ has no annotations: fields are bound explicitly with
 * ConfigurableProcessor::bind (Java: {@code @Property(key, defaultValue)}) and ConfigurableProcessor::bindPattern (Java:
 * {@code @Properties(keyPattern)}, not to be confused with the java.util.Properties port in Properties.h).
 * <p>
 * Java: com.aionemu.commons.configuration.Property
 */
struct Property {
	/**
	 * This string shows ConfigurableProcessor that the initial value of the field should not be overridden. It is the default value of
	 * ConfigurableProcessor::bind without an explicit default value. A property value (or placeholder result) equal to this string also leaves
	 * the field unmodified, like in Java.
	 */
	static constexpr std::string_view DEFAULT_VALUE = "DO_NOT_OVERWRITE_INITIALIAZION_VALUE";
};

} // namespace aion::commons::configuration
