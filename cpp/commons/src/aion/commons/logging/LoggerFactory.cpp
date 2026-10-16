#include "aion/commons/logging/LoggerFactory.h"

#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>

#include <spdlog/sinks/stdout_sinks.h>

namespace aion::commons::logging::LoggerFactory {

namespace {

bool isSameOrDescendant(std::string_view loggerName, std::string_view name) noexcept {
	return loggerName.starts_with(name) && (loggerName.size() == name.size() || loggerName[name.size()] == '.');
}

struct Registry {
	std::mutex mutex;
	std::shared_ptr<spdlog::sinks::dist_sink_mt> root;
	spdlog::level::level_enum rootLevel = spdlog::level::info;
	std::map<std::string, LoggerConfig, std::less<>> configs;
	struct Entry {
		std::shared_ptr<spdlog::logger> logger;
		/**
		 * The only sink of the logger, created with it. Reconfiguring replaces the sinks of this distributing sink under its lock, so the sink list
		 * of the spdlog::logger itself (which spdlog iterates without a lock) never changes.
		 */
		std::shared_ptr<spdlog::sinks::dist_sink_mt> sinks;
	};
	std::unordered_map<std::string, Entry> loggers;

	Registry() : root(std::make_shared<spdlog::sinks::dist_sink_mt>()) {
		// until Logging::init is called, log to stderr so early errors are not lost
		root->add_sink(std::make_shared<spdlog::sinks::stderr_sink_mt>());
	}

	/**
	 * Applies the effective configuration (logback: effective level and the appenders of all ancestors up to additivity="false").
	 *
	 * @param removedSinks receives the previous sinks, so they are destroyed after the registry lock is released
	 */
	void apply(Entry& entry, std::vector<spdlog::sink_ptr>& removedSinks) const {
		std::optional<spdlog::level::level_enum> level;
		std::vector<spdlog::sink_ptr> sinks;
		bool additive = true;
		std::string_view name = entry.logger->name();
		while (true) {
			if (auto it = configs.find(name); it != configs.end()) {
				const LoggerConfig& config = it->second;
				if (!level)
					level = config.level;
				if (additive)
					sinks.insert(sinks.end(), config.sinks.begin(), config.sinks.end());
				additive = additive && config.additive;
				if (level && !additive)
					break;
			}
			size_t dot = name.rfind('.');
			if (dot == std::string_view::npos)
				break;
			name = name.substr(0, dot);
		}
		if (additive)
			sinks.push_back(root);
		std::vector<spdlog::sink_ptr>& current = entry.sinks->sinks(); // only modified under the registry lock, so reading it here is safe
		removedSinks.insert(removedSinks.end(), current.begin(), current.end());
		entry.sinks->set_sinks(std::move(sinks));
		entry.logger->set_level(level.value_or(rootLevel));
	}

	/** @param name the configured name whose subtree changed, empty for all loggers */
	void applyAll(std::string_view name, std::vector<spdlog::sink_ptr>& removedSinks) {
		for (auto& [loggerName, entry] : loggers) {
			if (name.empty() || isSameOrDescendant(loggerName, name))
				apply(entry, removedSinks);
		}
	}
};

Registry& registry() {
	// Deliberately leaked: loggers are used by other static objects and threads (e.g. an appender's worker thread) during static destruction
	static Registry* instance = new Registry();
	return *instance;
}

} // namespace

Logger getLogger(std::string_view name) {
	Registry& r = registry();
	std::vector<spdlog::sink_ptr> removedSinks;
	std::lock_guard lock(r.mutex);
	auto it = r.loggers.find(std::string(name));
	if (it == r.loggers.end()) {
		Registry::Entry entry{.sinks = std::make_shared<spdlog::sinks::dist_sink_mt>()};
		entry.logger = std::make_shared<spdlog::logger>(std::string(name), entry.sinks);
		// flush warnings and errors immediately, so they are not lost if the process crashes
		entry.logger->flush_on(spdlog::level::warn);
		r.apply(entry, removedSinks);
		it = r.loggers.emplace(std::string(name), std::move(entry)).first;
	}
	return Logger(it->second.logger);
}

std::shared_ptr<spdlog::sinks::dist_sink_mt> rootSink() {
	return registry().root;
}

void setRootLevel(spdlog::level::level_enum level) {
	Registry& r = registry();
	std::vector<spdlog::sink_ptr> removedSinks; // destroyed after the lock is released
	std::lock_guard lock(r.mutex);
	r.rootLevel = level;
	r.applyAll({}, removedSinks);
}

void configure(std::string_view name, LoggerConfig config) {
	Registry& r = registry();
	std::vector<spdlog::sink_ptr> removedSinks; // destroyed after the lock is released, together with the replaced config
	LoggerConfig replaced;
	std::lock_guard lock(r.mutex);
	if (auto it = r.configs.find(name); it != r.configs.end())
		replaced = std::exchange(it->second, std::move(config));
	else
		r.configs.emplace(std::string(name), std::move(config));
	r.applyAll(name, removedSinks);
}

void removeConfig(std::string_view name) {
	Registry& r = registry();
	std::vector<spdlog::sink_ptr> removedSinks;
	LoggerConfig removed;
	std::lock_guard lock(r.mutex);
	auto it = r.configs.find(name);
	if (it == r.configs.end())
		return;
	removed = std::move(it->second);
	r.configs.erase(it);
	r.applyAll(name, removedSinks);
}

void flushAll() {
	Registry& r = registry();
	std::lock_guard lock(r.mutex);
	r.root->flush();
	for (auto& [name, entry] : r.loggers)
		entry.logger->flush();
}

} // namespace aion::commons::logging::LoggerFactory
