#include "aion/commons/configuration/ConfigurableProcessor.h"

#include <regex>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::configuration {

namespace {

const auto log = logging::LoggerFactory::getLogger("com.aionemu.commons.configuration.ConfigurableProcessor");

/**
 * Java: replacePropertyPlaceholders with the pattern \$\{([^}]+)\}. Like Java, matches are searched in the original value, and each match replaces
 * all occurrences of the complete token in the current value (which may include occurrences introduced by earlier replacements).
 */
std::string replacePropertyPlaceholders(std::string_view original, const Properties& props) {
	std::string value(original);
	std::size_t searchFrom = 0;
	while (true) {
		std::size_t start = original.find("${", searchFrom);
		if (start == std::string_view::npos)
			break;
		std::size_t end = original.find('}', start + 2);
		if (end == std::string_view::npos)
			break;                // [^}]+ never reaches a '}' anymore, no further match possible
		if (end == start + 2) { // "${}" does not match, retry at the next position
			searchFrom = start + 1;
			continue;
		}
		std::string_view completeToken = original.substr(start, end + 1 - start); // ${property.name}
		std::string_view token = original.substr(start + 2, end - start - 2);     // property.name
		std::optional<std::string> replacement = props.getProperty(token);
		value = utils::StringUtils::replace(value, completeToken, replacement ? *replacement : std::string_view());
		searchFrom = end + 1;
	}
	return value;
}

} // namespace

ConfigurableProcessor::ConfigurableProcessor(const Properties& props) : properties(props) {
	std::set<std::string> names = props.stringPropertyNames();
	unused.insert(names.begin(), names.end());
}

std::set<std::string> ConfigurableProcessor::process(const Properties& properties, std::initializer_list<Binder> binders) {
	return process(properties, std::span<const Binder>(binders.begin(), binders.size()));
}

std::set<std::string> ConfigurableProcessor::process(const Properties& properties, std::span<const Binder> binders) {
	ConfigurableProcessor processor(properties);
	for (const Binder& binder : binders)
		binder(processor);
	return processor.unusedProperties();
}

std::set<std::string> ConfigurableProcessor::unusedProperties() const {
	return std::set<std::string>(unused.begin(), unused.end());
}

std::string ConfigurableProcessor::getValue(std::string_view key, std::string_view defaultValue) {
	std::optional<std::string> property = properties.getProperty(key);
	// Java: if (!Objects.equals(value, defaultValue) || props.getProperty(key) != null), which is true exactly if the property exists
	if (property) {
		if (auto it = unused.find(key); it != unused.end())
			unused.erase(it);
	}
	std::string_view value = property ? std::string_view(*property) : defaultValue;
	if (utils::StringUtils::trim(value) == "\"\"")
		return std::string();
	return replacePropertyPlaceholders(value, properties);
}

std::map<std::string, std::string> ConfigurableProcessor::filterProperties(std::string_view keyPattern) {
	std::regex pattern(keyPattern.begin(), keyPattern.end(), std::regex::ECMAScript);
	bool hasGroup = pattern.mark_count() > 0;
	std::map<std::string, std::string> input;
	for (const std::string& k : properties.stringPropertyNames()) {
		std::smatch matcher;
		if (std::regex_search(k, matcher, pattern)) {
			std::string key = hasGroup ? matcher.str(1) : k;
			input.insert_or_assign(std::move(key), getValue(k, ""));
		}
	}
	return input;
}

void ConfigurableProcessor::logUnmodified(std::string_view key) const {
	log.debug("Field for property {} wasn't modified", key);
}

} // namespace aion::commons::configuration
