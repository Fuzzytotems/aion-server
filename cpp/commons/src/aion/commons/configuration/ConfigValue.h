#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace aion::commons::configuration {

/**
 * A config field whose value can be replaced while other threads read it.
 * <p>
 * The Java servers rebind config fields while they are running: the game server calls Config.load() when an event with config properties
 * starts or stops, on //reload config and in ChatProcessor.reload(), and //configure sets single fields. Java readers are memory safe during
 * such a reload because a reference field store is atomic: a reader sees either the old String/array/Map/Pattern or the new one. Assigning a
 * plain std::string, std::vector, std::map or std::regex while another thread uses it is undefined behaviour in C++ (the old buffer or nodes
 * are freed under the reader). ConfigValue restores Java's semantics: the value is an immutable object behind an atomically replaced
 * std::shared_ptr, and a reader keeps the snapshot it obtained alive for as long as it uses it.
 * <pre>
 * struct NameConfig {
 *   static inline ConfigValue&lt;std::optional&lt;std::wregex&gt;&gt; CHAR_NAME_PATTERN;
 *   static inline ConfigValue&lt;std::vector&lt;std::string&gt;&gt; FORBIDDEN_WORDS;
 * };
 * p.bind("gameserver.name.pattern", NameConfig::CHAR_NAME_PATTERN, "[a-zA-Z]{2,16}"); // ConfigurableProcessor stores a new snapshot
 *
 * auto words = NameConfig::FORBIDDEN_WORDS.get();  // snapshot, stays valid even if the config is reloaded meanwhile
 * for (const std::string&amp; word : *words) ...
 * </pre>
 * Never bind a temporary snapshot to a reference that outlives the full expression, e.g. {@code for (auto& w : *FORBIDDEN_WORDS.get())} dangles,
 * because the range-for keeps only the dereferenced object, not the shared_ptr. Keep the shared_ptr in a local variable instead.
 * <p>
 * Scalar fields that are rebound at runtime (bool, integers, floating point, enums) can be plain std::atomic&lt;T&gt; fields instead, which
 * ConfigurableProcessor binds as well. Fields that are only bound before other threads read them (e.g. database settings that are only read at
 * startup) can stay plain values.
 * <p>
 * Like in Java, a reload is not transactional: each field is replaced on its own, so readers can observe some fields of a reload before
 * others, and if a value fails to parse the fields bound before it keep their new values.
 * <p>
 * Not in Java (a plain static field there).
 *
 * @tparam T the value type, which must be supported by the config transformers to be bound
 */
template <typename T>
class ConfigValue {
public:
	/** A value-initialized T. */
	ConfigValue() : value(std::make_shared<const T>()) {}

	/** The initial value (Java: the field initializer). */
	explicit ConfigValue(T initialValue) : value(std::make_shared<const T>(std::move(initialValue))) {}

	ConfigValue(const ConfigValue&) = delete;
	ConfigValue& operator=(const ConfigValue&) = delete;

	/** @return a snapshot of the current value, never null. It is not affected by later calls to set. */
	std::shared_ptr<const T> get() const noexcept { return value.load(); }

	/** Atomically replaces the value (Java: assigning the field). Readers holding a snapshot of the old value keep it alive. */
	void set(T newValue) { value.store(std::make_shared<const T>(std::move(newValue))); }

private:
	std::atomic<std::shared_ptr<const T>> value;
};

} // namespace aion::commons::configuration
