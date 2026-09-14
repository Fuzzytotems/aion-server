#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

#include <algorithm>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"

namespace aion::gameserver::xml {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger =
	  new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.loadingutils.XmlDataLoader"));
	return *logger;
}

/** C++ type name without MSVC's "class "/"struct " prefixes (messages only) */
std::string typeName(std::type_index type) {
	std::string name = type.name();
	for (std::string_view prefix : {"class ", "struct ", "enum "}) {
		size_t pos;
		while ((pos = name.find(prefix)) != std::string::npos)
			name.erase(pos, prefix.size());
	}
	return name;
}

constexpr size_t MAX_LISTED_IDREF_ERRORS = 20;

} // namespace

// ---- ErasedHolder
// -------------------------------------------------------------------------------------------------------------------------------------

ErasedHolder::ErasedHolder(ErasedHolder&& other) noexcept
    : holderType(other.holderType), object(std::exchange(other.object, nullptr)), deleter(std::exchange(other.deleter, nullptr)) {}

ErasedHolder& ErasedHolder::operator=(ErasedHolder&& other) noexcept {
	if (this != &other) {
		if (object != nullptr && deleter != nullptr)
			deleter(object);
		holderType = other.holderType;
		object = std::exchange(other.object, nullptr);
		deleter = std::exchange(other.deleter, nullptr);
	}
	return *this;
}

ErasedHolder::~ErasedHolder() {
	if (object != nullptr && deleter != nullptr)
		deleter(object);
}

void ErasedHolder::checkType(std::type_index expected) const {
	if (expected != holderType)
		throw commons::utils::IllegalStateException("Holder is a " + typeName(holderType) + ", not a " + typeName(expected));
}

// ---- LoadContext
// --------------------------------------------------------------------------------------------------------------------------------------

LoadContext::LoadContext(LoadOptions options) : loadOptions(std::move(options)) {}

LoadContext::~LoadContext() = default;

bool LoadContext::hasHolder(std::string_view rootTag) const noexcept {
	return std::ranges::any_of(holders, [&](const auto& entry) { return entry.first == rootTag; });
}

std::vector<std::string> LoadContext::holderTags() const {
	std::vector<std::string> tags;
	tags.reserve(holders.size());
	for (const auto& [tag, holder] : holders)
		tags.push_back(tag);
	return tags;
}

const void* LoadContext::holderErased(std::type_index type, bool required) const {
	if (required && currentDependencies != nullptr && type != currentHolderType &&
	    std::ranges::find(*currentDependencies, type) == currentDependencies->end())
		throw commons::utils::IllegalStateException("Holder <" + currentHolderTag + "> reads " + typeName(type) +
		                                            ", which is not a declared dependency (HolderRegistration dependencies)");
	for (const auto& [tag, holder] : holders) {
		if (holder.type() == type)
			return holder.get();
	}
	if (publishedLookup) {
		if (const void* published = publishedLookup(type))
			return published;
	}
	if (required)
		throw commons::utils::IllegalStateException("Holder " + typeName(type) + " is neither loaded nor published");
	return nullptr;
}

ErasedHolder LoadContext::takeErased(std::type_index type) {
	auto it = std::ranges::find_if(holders, [&](const auto& entry) { return entry.second.type() == type; });
	if (it == holders.end())
		return {};
	ErasedHolder holder = std::move(it->second);
	holders.erase(it);
	return holder;
}

void LoadContext::beginHolder(std::string_view rootTag, std::type_index type, const std::vector<std::type_index>* dependencies) noexcept {
	currentHolderTag = rootTag;
	currentHolderType = type;
	currentDependencies = dependencies;
}

void LoadContext::endHolder() noexcept {
	currentHolderTag.clear();
	currentHolderType = typeid(void);
	currentDependencies = nullptr;
}

void LoadContext::addHolder(std::string rootTag, ErasedHolder holder) {
	auto it = std::ranges::find(holders, rootTag, &std::pair<std::string, ErasedHolder>::first);
	if (it == holders.end()) {
		holders.emplace_back(std::move(rootTag), std::move(holder));
		return;
	}
	std::string message = "Holder <" + rootTag + "> is imported more than once; the later import replaces the earlier one";
	if (loadOptions.strict)
		throw StaticDataException(message);
	log().warn(message);
	replacedHolders.push_back(std::move(it->second));
	holders.erase(it);
	holders.emplace_back(std::move(rootTag), std::move(holder));
}

std::vector<ErasedHolder> LoadContext::takeRetired() {
	std::vector<ErasedHolder> retired;
	retired.reserve(replacedHolders.size() + retiredObjects.size()); // the only allocation: nothing is moved out before it succeeded
	for (ErasedHolder& holder : replacedHolders)
		retired.push_back(std::move(holder));
	replacedHolders.clear();
	for (ErasedHolder& object : retiredObjects)
		retired.push_back(std::move(object));
	retiredObjects.clear();
	return retired;
}

uint32_t LoadContext::internFile(const std::string& displayName) {
	auto it = fileIndexes.find(displayName);
	if (it != fileIndexes.end())
		return it->second;
	auto index = static_cast<uint32_t>(fileNames.size());
	fileNames.push_back(displayName);
	fileIndexes.emplace(displayName, index);
	return index;
}

void LoadContext::fail(std::string_view message) const {
	if (activeBinding != nullptr)
		activeBinding->fail(message);
	throw StaticDataException(std::string(message));
}

void LoadContext::warn(std::string_view message) const {
	std::string where = currentLocation();
	log().warn(where.empty() ? std::string(message) : where + ": " + std::string(message));
}

bool LoadContext::warnOnce(std::string_view key, std::string_view message) {
	if (warnedKeys.contains(key))
		return false;
	warnedKeys.emplace(key);
	warn(message);
	return true;
}

std::string LoadContext::currentLocation() const {
	return activeBinding == nullptr ? std::string() : activeBinding->describeCurrent();
}

std::pair<uint32_t, XmlLocation> LoadContext::currentPosition() const {
	return activeBinding == nullptr ? std::pair<uint32_t, XmlLocation>{UINT32_MAX, {}} : activeBinding->currentPosition();
}

std::string LoadContext::describe(uint32_t file, XmlLocation location) const {
	if (file >= fileNames.size())
		return "<unknown location>";
	if (!location.known())
		return fileNames[file];
	return fileNames[file] + ":" + std::to_string(location.line) + ":" + std::to_string(location.column);
}

void LoadContext::registerXmlIdErased(std::string_view id, std::type_index type, const void* object) {
	std::string_view key = trimXml(id);
	auto [file, location] = currentPosition();
	auto it = xmlIds.find(key);
	if (it != xmlIds.end()) {
		std::string message = "Duplicate XmlID '" + std::string(key) + "' (" + typeName(type) + "), first declared at " +
		                      describe(it->second.file, it->second.location) + " (" + typeName(it->second.type) + ")";
		if (loadOptions.strict)
			fail(message);
		warn(message + "; the last one wins");
		if (activeBinding != nullptr)
			xmlIdJournal.push_back(JournalEntry{it->first, it->second});
		it->second = XmlIdTarget{type, object, file, location};
		return;
	}
	auto inserted = xmlIds.emplace(std::string(key), XmlIdTarget{type, object, file, location}).first;
	if (activeBinding != nullptr)
		xmlIdJournal.push_back(JournalEntry{inserted->first, std::nullopt});
}

void LoadContext::rollback(const Checkpoint& checkpoint) noexcept {
	while (xmlIdJournal.size() > checkpoint.journal) {
		JournalEntry& entry = xmlIdJournal.back();
		auto it = xmlIds.find(entry.key);
		if (it != xmlIds.end()) {
			if (entry.previous)
				it->second = *entry.previous;
			else
				xmlIds.erase(it);
		}
		xmlIdJournal.pop_back();
	}
	if (idRefs.size() > checkpoint.idRefs)
		idRefs.erase(idRefs.begin() + static_cast<std::ptrdiff_t>(checkpoint.idRefs), idRefs.end());
	if (afterIdRefTasks.size() > checkpoint.afterIdRefTasks)
		afterIdRefTasks.erase(afterIdRefTasks.begin() + static_cast<std::ptrdiff_t>(checkpoint.afterIdRefTasks), afterIdRefTasks.end());
}

void LoadContext::addIdRefErased(void* target, size_t index, std::type_index expected, std::string_view id,
                                 void (*patch)(void* target, size_t index, const void* object) noexcept) {
	auto [file, location] = currentPosition();
	idRefs.push_back(PendingIdRef{target, index, expected, patch, std::string(trimXml(id)), file, location});
}

const void* LoadContext::findXmlIdErased(std::string_view id, std::type_index type) const {
	auto it = xmlIds.find(trimXml(id));
	return it != xmlIds.end() && it->second.type == type ? it->second.object : nullptr;
}

void LoadContext::resolveIdRefs() {
	std::vector<std::string> errors;
	size_t errorCount = 0;
	for (const PendingIdRef& ref : idRefs) {
		auto it = xmlIds.find(ref.id);
		std::string error;
		if (it == xmlIds.end())
			error = "Unresolved IDREF '" + ref.id + "' (" + typeName(ref.expected) + ")";
		else if (it->second.type != ref.expected)
			error = "IDREF '" + ref.id + "' refers to a " + typeName(it->second.type) + " declared at " + describe(it->second.file, it->second.location) +
			        ", expected " + typeName(ref.expected);
		if (error.empty()) {
			ref.patch(ref.target, ref.index, it->second.object);
			continue;
		}
		if (++errorCount <= MAX_LISTED_IDREF_ERRORS)
			errors.push_back(describe(ref.file, ref.location) + ": " + error);
	}
	idRefs.clear();
	if (errorCount > 0) {
		std::string message = std::to_string(errorCount) + " unresolved IDREF(s):";
		for (const std::string& error : errors)
			message += "\n  " + error;
		if (errorCount > errors.size())
			message += "\n  ... and " + std::to_string(errorCount - errors.size()) + " more";
		throw StaticDataException(message);
	}
	std::vector<std::function<void()>> tasks = std::move(afterIdRefTasks);
	afterIdRefTasks.clear();
	for (const auto& task : tasks)
		task();
}

} // namespace aion::gameserver::xml
