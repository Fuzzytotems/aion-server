#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/BindStats.h"
#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/dataholders/loadingutils/XmlParent.h"

namespace aion::gameserver::xml {

/** Options of one static data load (docs/design/static-data.md §3.2 `options()`). */
struct LoadOptions {
	/**
	 * true (tests, CI, default): unknown attributes, unexpected text, repeated single elements (also @XmlElementWrapper wrappers), duplicate
	 * XmlIDs and holder tags imported twice are errors. false (server runtime): they are logged once per (class, name) and loading continues
	 * with JAXB's behaviour (objects replaced by a repeated element stay alive, see LoadContext::retire).
	 * Unknown elements, missing required attributes/elements, malformed values and unresolved IDREFs are errors in both modes.
	 */
	bool strict = true;
	/** root tags of the holders to load (tests); std::nullopt loads every import */
	std::optional<std::set<std::string, std::less<>>> holders;
	/** false skips afterUnmarshal hooks (value round-trip dump V5); generated finish() checks BindContext::hooksEnabled() */
	bool runHooks = true;
	/** collect BindStats (verification V3) */
	bool collectStats = false;
	/** GSConfig.SERVER_COUNTRY_CODE for the per-import region override (1 usa, 2 europe, 4 japan, 5 china, 6 taiwan, 7 russia) */
	int32_t countryCode = 0;
	/** parse the files of one import in parallel on the ForkJoin pool (binding is always sequential in import order) */
	bool parallelParse = true;
};

/** Type-erased owning pointer to a bound holder (or to another bound object kept alive by LoadContext::retire). */
class ErasedHolder {
public:
	ErasedHolder() noexcept = default;

	template <class H>
	explicit ErasedHolder(std::unique_ptr<H> holder) noexcept
	    : holderType(typeid(H)), object(holder.release()), deleter([](void* p) noexcept { delete static_cast<H*>(p); }) {}

	ErasedHolder(ErasedHolder&& other) noexcept;
	ErasedHolder& operator=(ErasedHolder&& other) noexcept;
	~ErasedHolder();

	std::type_index type() const noexcept { return holderType; }
	void* get() const noexcept { return object; }
	explicit operator bool() const noexcept { return object != nullptr; }

	/** takes ownership back as H; @throws IllegalStateException if the holder is not an H */
	template <class H>
	std::unique_ptr<H> take() {
		checkType(typeid(H));
		deleter = nullptr;
		return std::unique_ptr<H>(static_cast<H*>(std::exchange(object, nullptr)));
	}

private:
	void checkType(std::type_index expected) const;

	std::type_index holderType = typeid(void);
	void* object = nullptr;
	void (*deleter)(void*) noexcept = nullptr;
};

/**
 * State of one static data load, handed to hand-ported hooks (Java: the Unmarshaller plus StaticDataListener; docs/design/static-data.md §3.2).
 *
 * - Holders: the loader adds each finished holder in import order. Hooks read earlier holders with holder<H>() (a declared dependency of the
 *   holder being bound, HolderRegistration::dependencies); when H is not part of this load the published lookup is asked (DataManager,
 *   mirroring `staticData != null ? staticData.itemData : DataManager.ITEM_DATA`). DataManager takes the holders with takeHolder<H>().
 * - XmlID / XmlIDREF: one document-wide string ID space (items and npcs share it, as in JAXB). IDREF slots are recorded during binding and
 *   patched by resolveIdRefs() after all holders are loaded; an unresolved id or a target of another type is an error (all of them are listed).
 *   Registered objects must not move afterwards (holders keep their bound storage; hooks never move templates).
 * - Tasks: runAfterUnmarshalTask runs inline (Java registerForAsyncExecutionOrRun; deterministic NpcData.init), runAfterIdRefResolution runs
 *   after patching, in registration order (NpcEquippedGear eager initialization).
 * - Diagnostics: fail/warn/warnOnce prefix the location of the element being bound, if any.
 *
 * Confinement: K5, used by the loading thread only (the parallel parse phase does not touch it). Holders, ids and tasks refer to objects
 * owned by the holders, so the context must not outlive a holder it has handed out with takeHolder while IDREFs are still pending.
 */
class LoadContext {
public:
	explicit LoadContext(LoadOptions options = {});
	~LoadContext();
	LoadContext(const LoadContext&) = delete;
	LoadContext& operator=(const LoadContext&) = delete;

	const LoadOptions& options() const noexcept { return loadOptions; }
	BindStats& stats() noexcept { return bindStats; }
	const BindStats& stats() const noexcept { return bindStats; }

	// ---- root parent (Java: the StaticData object is the parent of every holder) ----------------------------------------------------------
	void setRoot(XmlParent parent) noexcept { rootParent = parent; }
	const XmlParent& root() const noexcept { return rootParent; }

	// ---- holders ----------------------------------------------------------------------------------------------------------------------------
	/**
	 * A finished holder of this load, or the published one. @throws IllegalStateException if H is not a declared dependency of the holder
	 * currently being bound, or if it is neither loaded nor published
	 */
	template <class H>
	const H* holder() const {
		return static_cast<const H*>(holderErased(typeid(H), true));
	}
	/** like holder<H>() but returns nullptr instead of throwing when H is not available (no dependency check) */
	template <class H>
	const H* findHolder() const {
		return static_cast<const H*>(holderErased(typeid(H), false));
	}
	/** removes the loaded holder H and returns it (nullptr if it was not loaded) */
	template <class H>
	std::unique_ptr<H> takeHolder() {
		ErasedHolder erased = takeErased(typeid(H));
		return erased ? erased.template take<H>() : nullptr;
	}
	bool hasHolder(std::string_view rootTag) const noexcept;
	/** root tags of the loaded (not yet taken) holders, in import order */
	std::vector<std::string> holderTags() const;
	/** fallback for holder<H>() when H is not loaded by this context (DataManager's published holders) */
	void setPublishedHolderLookup(std::function<const void*(std::type_index)> lookup) { publishedLookup = std::move(lookup); }

	// ---- hook services ------------------------------------------------------------------------------------------------------------------------
	/** Java StaticDataListener.registerForAsyncExecutionOrRun: runs the task inline */
	void runAfterUnmarshalTask(const std::function<void()>& task) { task(); }
	/** runs after resolveIdRefs() patched every IDREF, in registration order */
	void runAfterIdRefResolution(std::function<void()> task) { afterIdRefTasks.push_back(std::move(task)); }
	/** @throws StaticDataException with the current binding location */
	[[noreturn]] void fail(std::string_view message) const;
	/** logs a warning with the current binding location */
	void warn(std::string_view message) const;
	/** logs a warning once per key for this context (e.g. "ItemTemplate@name"); returns true if it was logged */
	bool warnOnce(std::string_view key, std::string_view message);
	/** "file:line:col: path" of the element being bound, empty outside binding */
	std::string currentLocation() const;

	// ---- XmlID / XmlIDREF ----------------------------------------------------------------------------------------------------------------------
	/** @XmlID: registers `object` under `id` (XML whitespace trimmed). Duplicate: error in strict mode, otherwise warning and last wins */
	template <class T>
	void registerXmlId(std::string_view id, const T& object) {
		registerXmlIdErased(id, typeid(T), &object);
	}
	/** @XmlIDREF single slot: sets it to nullptr now, patched by resolveIdRefs() */
	template <class T>
	void addIdRef(const T*& slot, std::string_view id) {
		slot = nullptr;
		addIdRefErased(&slot, 0, typeid(T), id,
		               [](void* target, size_t, const void* object) noexcept { *static_cast<const T**>(target) = static_cast<const T*>(object); });
	}
	/** @XmlIDREF list element: appends nullptr now, patched by resolveIdRefs() (the vector itself must not move until then) */
	template <class T>
	void addIdRef(std::vector<const T*>& list, std::string_view id) {
		list.push_back(nullptr);
		addIdRefErased(&list, list.size() - 1, typeid(T), id, [](void* target, size_t index, const void* object) noexcept {
			(*static_cast<std::vector<const T*>*>(target))[index] = static_cast<const T*>(object);
		});
	}
	/** the object registered under an id if it is a T, otherwise nullptr */
	template <class T>
	const T* findXmlId(std::string_view id) const {
		return static_cast<const T*>(findXmlIdErased(id, typeid(T)));
	}
	/** patches every recorded IDREF, then runs the runAfterIdRefResolution tasks. @throws StaticDataException listing unresolved references */
	void resolveIdRefs();
	size_t pendingIdRefs() const noexcept { return idRefs.size(); }
	size_t xmlIdCount() const noexcept { return xmlIds.size(); }

	// ---- runtime internals (StaticDataLoader, BindContext) ------------------------------------------------------------------------------------
	/** marks the holder whose root is being bound (dependency checks of holder<H>()); nullptr dependencies disable the check */
	void beginHolder(std::string_view rootTag, std::type_index type, const std::vector<std::type_index>* dependencies) noexcept;
	void endHolder() noexcept;
	/** adds a finished holder; a second holder with the same root tag replaces the first (error in strict mode) */
	void addHolder(std::string rootTag, ErasedHolder holder);
	/** registers the binding context whose location prefixes messages; returns the previous one */
	BindContext* setBinding(BindContext* binding) noexcept { return std::exchange(activeBinding, binding); }
	BindContext* binding() const noexcept { return activeBinding; }
	/** index of a file name in the location table */
	uint32_t internFile(const std::string& displayName);
	/** state to roll back to when a binding fails (BindContext saves it on construction) */
	struct Checkpoint {
		size_t idRefs = 0;
		size_t journal = 0;
		size_t afterIdRefTasks = 0;
	};
	Checkpoint checkpoint() const noexcept { return Checkpoint{idRefs.size(), xmlIdJournal.size(), afterIdRefTasks.size()}; }
	/**
	 * Undoes the XmlIDs, IDREFs and after-IDREF tasks registered since `checkpoint` (a failed binding destroys the objects they point to,
	 * so a context stays usable after an error). Holders are only added after successful binding; BindStats keep the partial counts.
	 */
	void rollback(const Checkpoint& checkpoint) noexcept;
	/** forgets the undo journal (the outermost binding finished successfully) */
	void commit() noexcept { xmlIdJournal.clear(); }
	/**
	 * Keeps a bound object that a repeated element replaced (lenient mode: bindSingle, single bindChoice, bindWrapper) alive instead of
	 * destroying it: while it was bound it may have registered XmlIDs, IDREF slots and runAfterIdRefResolution tasks that point into it (in
	 * Java the garbage collector keeps it). Null objects are ignored.
	 */
	template <class T>
	void retire(std::unique_ptr<T> object) {
		if (object != nullptr)
			retiredObjects.emplace_back(std::move(object));
	}
	/**
	 * Takes the objects this context keeps alive for pointers that may still reach them: holders replaced by a second import of their tag and
	 * objects replaced by repeated elements (both only in lenient mode). Resolved IDREFs of published holders can point into them, so
	 * DataManager::init keeps them for the life of the process, like published holders.
	 */
	std::vector<ErasedHolder> takeRetired();
	size_t retiredCount() const noexcept { return replacedHolders.size() + retiredObjects.size(); }

private:
	struct XmlIdTarget {
		std::type_index type;
		const void* object;
		uint32_t file;
		XmlLocation location;
	};
	struct PendingIdRef {
		void* target;
		size_t index;
		std::type_index expected;
		void (*patch)(void* target, size_t index, const void* object) noexcept;
		std::string id;
		uint32_t file;
		XmlLocation location;
	};
	struct StringHash {
		using is_transparent = void;
		size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
	};

	const void* holderErased(std::type_index type, bool required) const;
	ErasedHolder takeErased(std::type_index type);
	void registerXmlIdErased(std::string_view id, std::type_index type, const void* object);
	void addIdRefErased(void* target, size_t index, std::type_index expected, std::string_view id,
	                    void (*patch)(void* target, size_t index, const void* object) noexcept);
	const void* findXmlIdErased(std::string_view id, std::type_index type) const;
	std::pair<uint32_t, XmlLocation> currentPosition() const;
	std::string describe(uint32_t file, XmlLocation location) const;

	LoadOptions loadOptions;
	BindStats bindStats;
	XmlParent rootParent;
	std::vector<std::pair<std::string, ErasedHolder>> holders;
	/** holders replaced by a second import of the same tag (lenient mode): kept alive, their XmlIDs may be referenced */
	std::vector<ErasedHolder> replacedHolders;
	/** objects replaced by repeated elements (lenient mode), see retire() */
	std::vector<ErasedHolder> retiredObjects;
	std::function<const void*(std::type_index)> publishedLookup;
	std::string currentHolderTag;
	std::type_index currentHolderType = typeid(void);
	const std::vector<std::type_index>* currentDependencies = nullptr;
	BindContext* activeBinding = nullptr;
	std::unordered_map<std::string, XmlIdTarget, StringHash, std::equal_to<>> xmlIds;
	std::vector<PendingIdRef> idRefs;
	std::vector<std::function<void()>> afterIdRefTasks;
	/** undo log of registerXmlId while bindings are active: the key and the target it replaced (none for a new key) */
	struct JournalEntry {
		std::string key;
		std::optional<XmlIdTarget> previous;
	};
	std::vector<JournalEntry> xmlIdJournal;
	std::vector<std::string> fileNames;
	std::unordered_map<std::string, uint32_t, StringHash, std::equal_to<>> fileIndexes;
	std::unordered_set<std::string, StringHash, std::equal_to<>> warnedKeys;
};

} // namespace aion::gameserver::xml
