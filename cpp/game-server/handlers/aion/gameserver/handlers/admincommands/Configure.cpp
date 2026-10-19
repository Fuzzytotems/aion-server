#include "aion/gameserver/handlers/admincommands/Configure.h"

#include <algorithm>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/TransformationException.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/Config.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Configure);

// Java Configure.java:23-30
Configure::Configure()
	: AdminCommand("configure", "Shows/changes config settings.",
		  "list - Shows all available configuration categories.\n"
		  "<category> - Shows all available properties of the specified configuration.\n"
		  "<category> <property> - Shows the properties active value.\n"
		  "<category> <property> <value> - Changes the properties value to the new value.\n") {
}

// Java Configure.java:32-95
void Configure::execute(Player& admin, std::span<const std::string> params) {
	using commons::configuration::ConfigurableProcessor;
	using commons::configuration::TransformationException;
	using configs::Config;
	using ConfigClass = Config::ConfigClass;
	namespace StringUtils = commons::utils::StringUtils;

	if (params.empty()) { // parity= if (params.length == 0) {
		sendInfo(admin);
		return;
	}

	// parity= Map<String, Class<?>> configs = Config.getClasses().stream().sorted(Comparator.comparing(Class::getSimpleName, String.CASE_INSENSITIVE_ORDER)).collect(Collectors.toMap(cls -> cls.getSimpleName().toLowerCase().replace("config", ""), cls -> cls, (u, v) -> u, LinkedHashMap::new));
	// C++: the stream as a stable sort (Java's sorted is stable) and a vector in insertion order (Java's LinkedHashMap), the first of equal keys kept
	std::vector<ConfigClass> sorted(Config::getClasses().begin(), Config::getClasses().end()); // parity: (above)
	std::ranges::stable_sort(sorted, [](const auto& a, const auto& b) { // parity: (above)
		return StringUtils::toLowerCase(StringUtils::toUpperCase(a.simpleName)) < // parity: (above)
			StringUtils::toLowerCase(StringUtils::toUpperCase(b.simpleName)); // parity: (above)
	}); // parity: (above)
	std::vector<std::pair<std::string, ConfigClass>> configs; // parity: (above)
	for (const ConfigClass& cls : sorted) { // parity: (above)
		std::string key = StringUtils::replace(StringUtils::toLowerCase(cls.simpleName), "config", ""); // parity: (above)
		if (std::ranges::none_of(configs, [&key](const auto& entry) { return entry.first == key; })) // parity: (above)
			configs.emplace_back(std::move(key), cls); // parity: (above)
	} // parity: (above)
	if (StringUtils::equalsIgnoreCase("list", params[0])) { // parity= if ("list".equalsIgnoreCase(params[0])) {
		std::string sb = "List of available configuration names:"; // parity= StringBuilder sb = new StringBuilder("List of available configuration names:");
		for (const auto& [configname, cls] : configs) // parity= for (String configname : configs.keySet())
			sb.append("\n\t").append(configname);
		sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
	} else {
		const std::string requested = StringUtils::toLowerCase(params[0]); // parity: (below)
		auto found = std::ranges::find_if(configs, [&requested](const auto& entry) { return entry.first == requested; }); // parity= Class<?> cls = configs.get(params[0].toLowerCase());
		if (found == configs.end()) { // parity= if (cls == null) {
			sendInfo(admin, "Invalid configuration name. You can get a list of available configuration categories via the <list> parameter.");
			return;
		}
		const ConfigClass& cls = found->second; // parity: (the configs.get above)
		const std::string simpleName(cls.simpleName); // parity: Java's cls.getSimpleName() below
		// C++: findStaticFields replaced by the named binds of the class, in bind order (a bind without a name binds a local, not a field)
		std::vector<ConfigurableProcessor::FieldInfo> fields = ConfigurableProcessor::describe(cls.bind); // parity: (findStaticFields below)
		std::erase_if(fields, [](const ConfigurableProcessor::FieldInfo& field) { return field.fieldName.empty(); }); // parity: (the same)
		if (params.size() < 2) { // parity= if (params.length < 2) {
			std::string sb = "List of available properties for " + simpleName + ":"; // parity= StringBuilder sb = new StringBuilder("List of available properties for ").append(cls.getSimpleName()).append(":");
			for (const ConfigurableProcessor::FieldInfo& field : fields) { // parity= for (Field field : findStaticFields(cls, Predicates.alwaysTrue())) {
				std::string value = field.toJavaString(); // parity= try { String value = getFieldValue(field);
				sb.append("\n\t").append(field.fieldName).append("\t=\t").append(value); // parity= sb.append("\n\t").append(field.getName()).append("\t=\t").append(value);
				// parity= } catch (IllegalArgumentException | IllegalAccessException e) { sb.append("\n\t").append(field.getName()).append("\t=\t").append("Error reading value: ").append(e.getMessage()); } // reading a bound field cannot fail in C++
			}
			sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
			return;
		}
		const std::string fieldName = StringUtils::toUpperCase(params[1]); // parity= String fieldName = params[1].toUpperCase();
		try {
			std::erase_if(fields, [&fieldName](const ConfigurableProcessor::FieldInfo& field) { return field.fieldName != fieldName; }); // parity= List<Field> fields = findStaticFields(cls, field -> field.getName().equals(fieldName));
			if (fields.empty()) { // parity= if (fields.isEmpty()) {
				sendInfo(admin, simpleName + "." + fieldName + " does not exist."); // parity= sendInfo(admin, cls.getSimpleName() + "." + fieldName + " does not exist.");
				return;
			}
			const ConfigurableProcessor::FieldInfo& field = fields[0]; // parity= Field field = fields.get(0);
			std::string value = field.toJavaString(); // parity= String value = getFieldValue(field);
			if (params.size() > 2) { // parity= if (params.length > 2) {
				std::string newValue = join(params, 2); // parity= String newValue = join(params, 2);
				// C++: a field that is neither a std::atomic nor a ConfigValue cannot be stored into while other threads read it
				if (field.isPattern ? !field.setMap : !field.set) { // parity: (the C++ refusal above)
					sendInfo(admin, simpleName + "." + fieldName + " cannot be changed at runtime in the C++ port."); // parity: (the C++ refusal above)
					return; // parity: (the C++ refusal above)
				} // parity: (the C++ refusal above)
				try {
					if (field.isPattern) // parity= if (field.isAnnotationPresent(Properties.class))
						field.setMap(toMap(newValue)); // parity= field.set(null, ConfigurableProcessor.transform(toMap(newValue), field));
					else
						field.set(newValue); // parity= field.set(null, ConfigurableProcessor.transform(newValue, field));
				} catch (const TransformationException& e) {
					if (!e.cause()) // parity: (e.getCause() below)
						throw runtime::NullPointerException("TransformationException.getCause()"); // parity: (e.getCause() below)
					std::string causeMessage; // parity: (e.getCause() below)
					try { // parity: (e.getCause() below)
						std::rethrow_exception(e.cause()); // parity: (e.getCause() below)
					} catch (const std::exception& cause) { // parity: (e.getCause() below)
						causeMessage = cause.what(); // parity: (e.getCause() below)
					} // parity: (e.getCause() below)
					sendInfo(admin, "The new value could not be set: " + causeMessage); // parity= sendInfo(admin, "The new value could not be set: " + e.getCause().getMessage());
					return;
				}
				sendInfo(admin, "The value of " + simpleName + "." + fieldName + " has been changed from " + value + " to " + field.toJavaString()); // parity= sendInfo(admin, "The value of " + cls.getSimpleName() + "." + fieldName + " has been changed from " + value + " to " + getFieldValue(field));
			} else {
				sendInfo(admin, "The current value of " + simpleName + "." + fieldName + " is " + value); // parity= sendInfo(admin, "The current value of " + cls.getSimpleName() + "." + fieldName + " is " + value);
			}
		} catch (const std::exception&) { // parity= } catch (Exception e) {
			sendInfo(admin, "Could not access " + simpleName + "." + fieldName); // parity= sendInfo(admin, "Could not access " + cls.getSimpleName() + "." + fieldName);
		}
	}
}

// Java Configure.java:97-115
std::map<std::string, std::string> Configure::toMap(std::string value) {
	namespace StringUtils = commons::utils::StringUtils;
	std::map<std::string, std::string> values; // parity= Map<String, String> values = new LinkedHashMap<>();
	if (value.starts_with("{") && value.ends_with("}")) // parity= if (value.startsWith("{") && value.endsWith("}"))
		value = value.substr(1, value.size() - 2); // parity= value = value.substring(1, value.length() - 1);
	// C++: Java's String.split(regex): a separator at the start leaves an empty first entry, trailing empty entries are removed
	const std::regex separator(value.find(',') != std::string::npos ? " *, *" : " +"); // parity= String[] entries = value.contains(",") ? value.split(" *, *") : value.split(" +");
	std::vector<std::string> entries; // parity: (the split above)
	size_t start = 0; // parity: (the split above)
	for (std::sregex_iterator it(value.begin(), value.end(), separator), end; it != end; ++it) { // parity: (the split above)
		entries.push_back(value.substr(start, static_cast<size_t>(it->position()) - start)); // parity: (the split above)
		start = static_cast<size_t>(it->position() + it->length()); // parity: (the split above)
	} // parity: (the split above)
	entries.push_back(value.substr(start)); // parity: (the split above)
	while (entries.size() > 1 && entries.back().empty()) // parity: (the split above)
		entries.pop_back(); // parity: (the split above)
	if (entries.size() == 1 && entries[0].empty() && !value.empty()) // parity: (the split above)
		entries.clear(); // parity: (the split above)
	try {
		for (const std::string& entry : entries) { // parity= for (String entry : entries) {
			size_t indexOfEqualsSign = entry.find('='); // parity= int indexOfEqualsSign = entry.indexOf('=');
			if (indexOfEqualsSign == std::string::npos) // parity= if (indexOfEqualsSign == -1)
				throw commons::utils::IllegalArgumentException("Missing value after " + entry + " (format: key=value)"); // parity= throw new IllegalArgumentException("Missing value after " + entry + " (format: key=value)");
			values.insert_or_assign(std::string(StringUtils::trim(entry.substr(0, indexOfEqualsSign))), // parity= values.put(entry.substring(0, indexOfEqualsSign).trim(), entry.substring(indexOfEqualsSign + 1).trim());
				std::string(StringUtils::trim(entry.substr(indexOfEqualsSign + 1)))); // parity: (the same)
		}
	} catch (...) { // parity= } catch (Exception e) {
		throw commons::configuration::TransformationException("null", std::current_exception()); // parity= throw new TransformationException(null, e);
	}
	return values;
}

// Java Configure.java:117-136: getFieldValue and findStaticFields are not ported; FieldInfo::toJavaString and describe replace them
// parity= private String getFieldValue(Field field) throws IllegalArgumentException, IllegalAccessException { Object value = field.get(null);
// parity= if (value != null && value.getClass().isArray()) { if (value.getClass().getComponentType().isPrimitive()) { int length = Array.getLength(value);
// parity= Object[] objArr = new Object[length]; for (int i = 0; i < length; i++) objArr[i] = Array.get(value, i); value = Arrays.toString(objArr);
// parity= } else value = Arrays.toString((Object[]) value); } return String.valueOf(value); }
// parity= private static List<Field> findStaticFields(Class<?> cls, Predicate<Field> filter) {
// parity= return Arrays.stream(cls.getDeclaredFields()).filter(filter.and(field -> Modifier.isStatic(field.getModifiers()))).toList(); }

} // namespace aion::gameserver::handlers::admincommands
