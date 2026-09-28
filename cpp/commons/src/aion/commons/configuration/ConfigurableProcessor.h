#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/configuration/Properties.h"
#include "aion/commons/configuration/Property.h"
#include "aion/commons/configuration/TransformationException.h"
#include "aion/commons/configuration/transformers/PropertyTransformers.h"

namespace aion::commons::configuration {

/**
 * Binds property values to configuration fields and tracks which properties were used. This replaces the reflection-based processing of fields
 * annotated with {@code @Property} / {@code @Properties} in Java: each config class has a static bind function that binds its fields explicitly,
 * with the same keys and default values as the Java annotations.
 * <pre>
 * struct Config {
 *   static inline utils::InetSocketAddress CLIENT_SOCKET_ADDRESS;
 *   static inline int32_t LOGIN_TRY_BEFORE_BAN = 0;
 *   static inline std::string EXTERNAL_AUTH_URL;
 *
 *   static void bind(ConfigurableProcessor&amp; p) {
 *     p.bind("loginserver.network.client.socket_address", CLIENT_SOCKET_ADDRESS, "0.0.0.0:2106"); // @Property(key, defaultValue)
 *     p.bind("loginserver.network.client.logintrybeforeban", LOGIN_TRY_BEFORE_BAN, "5");
 *     p.bind("loginserver.accounts.external_auth.url", EXTERNAL_AUTH_URL, "");
 *   }
 * };
 * std::set&lt;std::string&gt; unused = ConfigurableProcessor::process(properties, {&amp;Config::bind, &amp;CommonsConfig::bind,
 * &amp;DatabaseConfig::bind});
 * </pre>
 * Value resolution for each bound key (identical to Java):
 * <ol>
 * <li>The value is looked up through the defaults chain of the Properties; if the key is missing, the default value is used.</li>
 * <li>If the key exists, it is removed from the unused properties.</li>
 * <li>If the trimmed value is {@code ""} (two double quotes), the value is the empty string. Otherwise every {@code ${name}} placeholder is replaced
 * by the value of property "name" (through the defaults chain; empty if missing). Replacements are not resolved recursively.</li>
 * <li>If the result equals Property::DEFAULT_VALUE (the default of bind without a default value), the field keeps its current value and a debug
 * message is logged. Otherwise the value is transformed and assigned (see transformers/PropertyTransformers.h for supported types).</li>
 * </ol>
 * Errors are thrown as TransformationException("Error modifying field for property &lt;key&gt;") with the transformer's exception
 * ("Error parsing \"&lt;value&gt;\" as &lt;type&gt;") as cause. A field is only assigned if its value could be transformed; fields bound before the
 * error keep their new values, like in Java.
 * <p>
 * Deviation: without reflection there are no field names, so the outer exception and the debug message for unmodified fields name the property key
 * instead of the field and class (Java: RuntimeException "Error modifying field X of class Y", debug "Field X of class Y wasn't modified").
 * Java's check for final fields is replaced by the compiler (const fields cannot be bound).
 * <p>
 * <b>Threads and reloading.</b> Like in Java, fields are assigned while binding, one after another. The Java servers do this at runtime too (game
 * server: Config.load() on event start/stop, //reload config, ChatProcessor.reload(), and //configure assigning single fields), while other
 * threads read the fields. That is memory safe in Java (reference stores are atomic), but assigning a plain std::string, container or regex
 * that another thread reads is undefined behaviour in C++. Therefore:
 * <ul>
 * <li>Fields that are rebound while other threads may read them must be ConfigValue&lt;T&gt; (any bindable type, readers take a snapshot with
 * get()) or std::atomic&lt;T&gt; (scalars). bind and bindPattern store into them atomically.</li>
 * <li>Plain fields may only be bound while no other thread reads them (e.g. at startup, or fields that are only read at startup).</li>
 * </ul>
 * Several processors may run concurrently (e.g. an event starting while an admin reloads the config); for ConfigValue and std::atomic fields the
 * last store wins, like in Java. A processor object itself must only be used by one thread. It refers to the Properties passed to the
 * constructor, which must outlive it.
 * <p>
 * Java: com.aionemu.commons.configuration.ConfigurableProcessor
 *
 * @author SoulKeeper
 */
class ConfigurableProcessor {
public:
	/** A function binding the fields of one config class (Java: the Class object passed to process). */
	using Binder = std::function<void(ConfigurableProcessor&)>;

	/** Starts processing: all property names (including defaults) are initially unused. */
	explicit ConfigurableProcessor(const Properties& properties);
	/** The processor refers to the properties, so they must not be a temporary. */
	explicit ConfigurableProcessor(Properties&&) = delete;

	/**
	 * Processes the bindings of the given config classes and parses corresponding values from the passed properties.
	 * <p>
	 * Java: ConfigurableProcessor.process(Properties, Object...)
	 *
	 * @return the names of all properties (including defaults) that were not used by any binding
	 */
	static std::set<std::string> process(const Properties& properties, std::initializer_list<Binder> binders);

	/** Like process(properties, {binders...}), for a dynamic list of config classes (e.g. game server's Config.load(allowedConfigs)). */
	static std::set<std::string> process(const Properties& properties, std::span<const Binder> binders);

	/** Java: {@code @Property(key = key)} without defaultValue. If the key is missing, the field keeps its current value. */
	template <transformers::Transformable T>
	void bind(std::string_view key, T& field) {
		bind(key, field, Property::DEFAULT_VALUE);
	}

	/** Java: {@code @Property(key = key, defaultValue = defaultValue)}. The default value is parsed like a property value. */
	template <transformers::Transformable T>
	void bind(std::string_view key, T& field, std::string_view defaultValue) {
		if (std::optional<T> value = parseValue<T>(key, defaultValue))
			field = std::move(*value);
	}

	/** Like bind(key, field) for a field that is rebound while other threads read it (see ConfigValue). */
	template <transformers::Transformable T>
	void bind(std::string_view key, ConfigValue<T>& field) {
		bind(key, field, Property::DEFAULT_VALUE);
	}

	/** Like bind(key, field, defaultValue) for a field that is rebound while other threads read it: stores a new snapshot (see ConfigValue). */
	template <transformers::Transformable T>
	void bind(std::string_view key, ConfigValue<T>& field, std::string_view defaultValue) {
		if (std::optional<T> value = parseValue<T>(key, defaultValue))
			field.set(std::move(*value));
	}

	/** Like bind(key, field) for a scalar field that is rebound while other threads read it. */
	template <transformers::Transformable T>
	void bind(std::string_view key, std::atomic<T>& field) {
		bind(key, field, Property::DEFAULT_VALUE);
	}

	/** Like bind(key, field, defaultValue) for a scalar field that is rebound while other threads read it: stores the new value atomically. */
	template <transformers::Transformable T>
	void bind(std::string_view key, std::atomic<T>& field, std::string_view defaultValue) {
		if (std::optional<T> value = parseValue<T>(key, defaultValue))
			field.store(*value);
	}

	/**
	 * Java: {@code @Properties(keyPattern = keyPattern)}. Replaces the map with all properties (including defaults) whose key contains a match of
	 * the regular expression (ECMAScript, std::regex_search like Java's Matcher.find). The map key is the content of capture group 1 if the pattern
	 * has a group (empty if the group did not participate in the match), else the whole property key. Values are resolved like in bind (without
	 * the Property::DEFAULT_VALUE check) and all matching keys are marked as used.
	 * <pre>
	 * p.bindPattern("^gameserver\\.topranking\\.quota\\.(.+)", TOP_RANKING_QUOTA); // std::map&lt;AbyssRankEnum, int32_t&gt;
	 * </pre>
	 * If several property keys yield the same map key, the value of the lexicographically last property key wins (Java: undefined).
	 */
	template <transformers::TransformableMap Map>
	void bindPattern(std::string_view keyPattern, Map& field) {
		field = parseMap<Map>(keyPattern);
	}

	/** Like bindPattern(keyPattern, field) for a map that is rebound while other threads read it: stores a new snapshot (see ConfigValue). */
	template <transformers::TransformableMap Map>
	void bindPattern(std::string_view keyPattern, ConfigValue<Map>& field) {
		field.set(parseMap<Map>(keyPattern));
	}

	/** @return the names of all properties (including defaults) that were not used by the bindings so far */
	std::set<std::string> unusedProperties() const;

	/**
	 * Transforms a value to the given type without binding (Java: ConfigurableProcessor.transform(String, Field), used by the //configure admin
	 * command to change config values at runtime). Store the result with ConfigValue::set or std::atomic::store if other threads read the field.
	 *
	 * @throws TransformationException "Error parsing \"&lt;value&gt;\" as &lt;type&gt;", whose cause describes the problem
	 */
	template <transformers::Transformable T>
	static T transform(std::string_view value) {
		return transformers::transform<T>(value);
	}

	/**
	 * Transforms (key, value) string pairs to a map (Java: ConfigurableProcessor.transform(Map, Field)).
	 *
	 * @throws TransformationException see MapTransformer::transform
	 */
	template <transformers::TransformableMap Map, std::ranges::input_range Values>
	static Map transformMap(const Values& values) {
		return transformers::MapTransformer::transform<Map>(values);
	}

private:
	/** @return the transformed value, or std::nullopt if the field must keep its current value (Property::DEFAULT_VALUE) */
	template <transformers::Transformable T>
	std::optional<T> parseValue(std::string_view key, std::string_view defaultValue) {
		std::string value = getValue(key, defaultValue);
		if (value == Property::DEFAULT_VALUE) {
			logUnmodified(key);
			return std::nullopt;
		}
		try {
			return std::optional<T>(std::in_place, transformers::transform<T>(value));
		} catch (...) {
			throw TransformationException("Error modifying field for property " + std::string(key), std::current_exception());
		}
	}

	template <transformers::TransformableMap Map>
	Map parseMap(std::string_view keyPattern) {
		try {
			std::map<std::string, std::string> values = filterProperties(keyPattern);
			return transformers::MapTransformer::transform<Map>(values);
		} catch (...) {
			throw TransformationException("Error modifying field for properties matching " + std::string(keyPattern), std::current_exception());
		}
	}

	std::string getValue(std::string_view key, std::string_view defaultValue);
	std::map<std::string, std::string> filterProperties(std::string_view keyPattern);
	void logUnmodified(std::string_view key) const;

	const Properties& properties;
	std::set<std::string, std::less<>> unused;
};

} // namespace aion::commons::configuration
