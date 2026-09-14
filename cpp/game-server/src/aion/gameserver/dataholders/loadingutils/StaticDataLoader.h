#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"

namespace aion::gameserver::xml {

/**
 * How the loader creates and binds the holder of one root tag (one @XmlElement field of the Java StaticData class). The generated StaticData
 * registry lists one per holder:
 * <pre>
 * registry.add(HolderRegistration::of<ItemData>("item_templates"));
 * registry.add(HolderRegistration::of<ItemGroupsData>("item_groups", {typeid(ItemData)}));   // its hooks read ctx.holder<ItemData>()
 * </pre>
 * `dependencies` is the hand-maintained table of holders whose hooks read other holders (docs/design/static-data.md §3.3); holder<H>() in a
 * hook of this holder throws for any type not listed. Sequential loading in import order satisfies it; a later parallel mode schedules by it.
 */
struct HolderRegistration {
	std::string rootTag;
	std::type_index type = typeid(void);
	std::string className;
	std::vector<std::type_index> dependencies;
	ErasedHolder (*create)() = nullptr;
	void (*bind)(BindContext& context, void* holder, std::span<const XmlDocument* const> documents, const XmlParent& parent) = nullptr;

	template <class H>
	static HolderRegistration of(std::string rootTag, std::vector<std::type_index> dependencies = {}) {
		HolderRegistration registration;
		registration.rootTag = std::move(rootTag);
		registration.type = typeid(H);
		registration.className = std::string(bindingClassName<H>());
		registration.dependencies = std::move(dependencies);
		registration.create = []() { return ErasedHolder(constructBound<H>()); };
		registration.bind = [](BindContext& context, void* holder, std::span<const XmlDocument* const> documents, const XmlParent& parent) {
			context.bindHolder(*static_cast<H*>(holder), documents, parent);
		};
		return registration;
	}
};

/** Root tag -> holder registration. */
class HolderRegistry {
public:
	/** @throws IllegalArgumentException if the root tag is already registered */
	void add(HolderRegistration registration);
	const HolderRegistration* find(std::string_view rootTag) const noexcept;
	const std::vector<HolderRegistration>& entries() const noexcept { return registrations; }

private:
	std::vector<HolderRegistration> registrations;
};

/**
 * Loads static data (Java: XmlDataLoader.loadStaticData = XmlMerger + one JAXB unmarshal; docs/design/static-data.md §3.2, amendments §4).
 *
 * For each import of static_data.xml, in document order:
 * 1. With LoadOptions::holders set, the root tag of the first file is peeked and imports of other holders are skipped without parsing.
 * 2. All files of the import are read and parsed, in parallel on ForkJoinPool::commonPool() when LoadOptions::parallelParse is set and there
 *    are several files (parsing only touches the documents, so helpers run in joined TaskScopes without pointer loads). The first parse error
 *    is thrown after all files finished.
 * 3. The root tag of the first file selects the HolderRegistration (an unregistered tag is an error: JAXB's unexpected element), a new holder
 *    is created and bound sequentially: XmlBinding<H>::reserve over all roots, the first root's attributes, the children of every file in
 *    order, finish (required checks and the holder's hook) once.
 * 4. The holder is added to the LoadContext, the DOMs are freed.
 *
 * The caller then resolves IDREFs (LoadContext::resolveIdRefs), post-processes and takes the holders (DataManager::init).
 *
 * <pre>
 * xml::LoadContext context(options);
 * context.setRoot(xml::XmlParent::of(staticData));
 * xml::StaticDataLoader(registry).load(context, "./data/static_data/static_data.xml");
 * context.resolveIdRefs();
 * staticData.itemData = context.takeHolder<ItemData>();
 * </pre>
 *
 * Thread-safety: one load at a time per LoadContext; binding runs on the calling thread.
 */
class StaticDataLoader {
public:
	explicit StaticDataLoader(const HolderRegistry& registry) noexcept : registry(registry) {}

	/** resolves the imports of static_data.xml and loads them. @throws StaticDataException */
	void load(LoadContext& context, const std::filesystem::path& staticDataXml) const;
	/** loads already resolved imports. @throws StaticDataException */
	void load(LoadContext& context, std::span<const StaticDataImport> imports) const;
	/** loads one holder from a list of files (singleRootTag semantics, also used for single files). @throws StaticDataException */
	void loadFiles(LoadContext& context, std::span<const std::filesystem::path> files) const;

	/** The tag of the first element of an XML file, read without parsing the whole file (empty if none was found in the first 64 KB). */
	static std::string peekRootTag(const std::filesystem::path& file);
	/** Parses files, in parallel on the ForkJoin pool if `parallel`; the result is in input order. @throws StaticDataException */
	static std::vector<std::unique_ptr<XmlDocument>> parseFiles(std::span<const std::filesystem::path> files, bool parallel);

private:
	void bindDocuments(LoadContext& context, std::span<const XmlDocument* const> documents) const;

	const HolderRegistry& registry;
};

/** Binds the root element of a parsed document as T with the context's root parent (tests, handler-private and config XML roots). */
template <class T>
std::unique_ptr<T> bindDocument(LoadContext& context, const XmlDocument& document) {
	BindContext binding(context);
	return binding.bindRoot<T>(document, context.root());
}

/** Parses and binds XML text as T. @throws StaticDataException */
template <class T>
std::unique_ptr<T> bindString(LoadContext& context, std::string_view xml, std::string displayName = "<memory>") {
	std::unique_ptr<XmlDocument> document = XmlDocument::parseString(xml, std::move(displayName));
	return bindDocument<T>(context, *document);
}

/** Parses and binds an XML file as T. @throws StaticDataException */
template <class T>
std::unique_ptr<T> bindFile(LoadContext& context, const std::filesystem::path& file) {
	std::unique_ptr<XmlDocument> document = XmlDocument::parseFile(file);
	return bindDocument<T>(context, *document);
}

} // namespace aion::gameserver::xml
