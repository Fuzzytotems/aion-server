#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/loadingutils/ElementFactory.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/XmlBinding.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/dataholders/loadingutils/XmlParent.h"
#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"

namespace aion::gameserver::xml {

namespace detail {
template <class T>
struct IsUniquePtr : std::false_type {};
template <class T>
struct IsUniquePtr<std::unique_ptr<T>> : std::true_type {};
} // namespace detail

/** A class type bound through XmlBinding<T> (not a scalar, not a container helper type). */
template <class T>
concept XmlObject = std::is_class_v<T> && !XmlScalar<T> && !Optional<T> && !detail::IsUniquePtr<T>::value && !detail::IsVector<T>::value;

/** T or std::optional<T> of a scalar: targets of assign/bindText */
template <class T>
concept XmlScalarTarget = XmlScalar<T> || (Optional<T> && XmlScalar<typename T::value_type>);

/** targets of assignList/bindTextList: a scalar vector/unordered_set, optionally wrapped in std::optional */
template <class C>
concept XmlListTarget = XmlScalarCollection<C> || (Optional<C> && XmlScalarCollection<typename C::value_type>);

/**
 * The binder runtime that generated XmlBinding<T> specializations call (the contract is documented in XmlBinding.h; design
 * docs/design/static-data.md §2.3-§3.2).
 *
 * One BindContext binds documents for one LoadContext on one thread. It walks the DOM post-order: for an object element it reserves lists
 * (XmlBinding<T>::reserve), binds attributes, then children in document order (each child completely, including its hook), then calls
 * XmlBinding<T>::finish. It counts every element and attribute (BindStats, when enabled), enforces the strict-mode rules and prefixes every
 * error with `file:line:col: path/of/elements (Class):`.
 *
 * Ignored without a binder call: xmlns / xmlns:* attributes and attributes whose prefix is bound to the XML Schema instance namespace
 * (xsi:noNamespaceSchemaLocation, ...). Other prefixed attributes are unknown. Comments and processing instructions never reach the binder.
 *
 * Confinement: K5 (loading thread only).
 */
class BindContext {
public:
	explicit BindContext(LoadContext& load);
	~BindContext();
	BindContext(const BindContext&) = delete;
	BindContext& operator=(const BindContext&) = delete;

	LoadContext& load() noexcept { return loadContext; }
	const LoadOptions& options() const noexcept { return loadContext.options(); }
	bool strict() const noexcept { return loadContext.options().strict; }
	/** false when hooks are disabled (LoadOptions::runHooks) */
	bool hooksEnabled() const noexcept { return loadContext.options().runHooks; }

	// ---- entry points (loader, bindDocument) ------------------------------------------------------------------------------------------------------
	/** Binds the root element of a document as a new T, its finish() gets `parent`. */
	template <class T>
	std::unique_ptr<T> bindRoot(const XmlDocument& document, const XmlParent& parent);
	/**
	 * Binds one holder from the roots of all files of an import (XmlMerger singleRootTag semantics): the root attributes of the first file;
	 * the root tags and attributes of later files are skipped (a different root tag is an error in strict mode); the children of all roots are
	 * bound in file order; finish() runs once at the end.
	 */
	template <class H>
	void bindHolder(H& holder, std::span<const XmlDocument* const> documents, const XmlParent& parent);
	/** Binds `node` into an existing object (used by the helpers below and hand-written adapters). */
	template <class T>
	void bindObject(T& object, pugi::xml_node node, const XmlParent& parent);

	// ---- attribute values (inside XmlBinding<T>::attribute) -------------------------------------------------------------------------------------
	/** converts a value; conversion errors carry the location of the attribute being bound */
	template <XmlScalar T>
	T value(std::string_view v) {
		return convert([&] { return parseValue<T>(v); });
	}
	int8_t parseInt8(std::string_view v) { return value<int8_t>(v); }
	int16_t parseInt16(std::string_view v) { return value<int16_t>(v); }
	int32_t parseInt32(std::string_view v) { return value<int32_t>(v); }
	int64_t parseInt64(std::string_view v) { return value<int64_t>(v); }
	float parseFloat(std::string_view v) { return value<float>(v); }
	double parseDouble(std::string_view v) { return value<double>(v); }
	bool parseBool(std::string_view v) { return value<bool>(v); }

	/** scalar attribute into T or std::optional<T> (an empty value into a non-optional std::string warns once per class and attribute) */
	template <XmlScalarTarget T>
	void assign(T& target, std::string_view v) {
		if constexpr (Optional<T>) {
			target = value<typename T::value_type>(v);
		} else {
			if constexpr (std::same_as<T, std::string>) {
				if (v.empty())
					warnEmptyString();
			}
			target = value<T>(v);
		}
	}
	/** @XmlList attribute or collection-typed attribute (space separated) */
	template <XmlListTarget C>
	void assignList(C& target, std::string_view v) {
		target = convert([&] { return parseList<C>(v); });
	}
	/**
	 * Marks the attribute being bound as deliberately ignored (xmlgen.toml [ignore_attributes]: present in the data, not bound by Java). The
	 * binder still returns true; BindStats count the attribute as ignored instead of bound. @throws IllegalStateException outside
	 * XmlBinding<T>::attribute
	 */
	void ignoreAttribute();
	/** adapter call with location: `o.start = c.adapt(adapters::parseLocalDateTime, v);` */
	template <class F>
	auto adapt(F&& adapter, std::string_view v) -> decltype(std::forward<F>(adapter)(v)) {
		return convert([&]() -> decltype(auto) { return std::forward<F>(adapter)(v); });
	}

	// ---- child elements (inside XmlBinding<T>::element) -----------------------------------------------------------------------------------------
	/**
	 * @XmlElement object: creates, binds, assigns (repeated element: error in strict mode, otherwise the last one wins and the replaced object
	 * is kept alive by LoadContext::retire)
	 */
	template <XmlObject T>
	void bindSingle(std::unique_ptr<T>& slot, pugi::xml_node element);
	/**
	 * Stores an object that hand-written or class-adapter code built from `element` (after bindObject) in a single slot, with bindSingle's
	 * rules for a repeated element: error in strict mode, otherwise the replaced object is kept alive by LoadContext::retire.
	 */
	template <class T>
	void replaceSingle(std::unique_ptr<T>& slot, std::unique_ptr<T> object, pugi::xml_node element) {
		checkRepeated(slot != nullptr, element);
		loadContext.retire(std::move(slot));
		slot = std::move(object);
	}
	/** List<T> of objects bound in place (the vector must have been reserved) or of scalars (text content) */
	template <class T>
	void bindList(std::vector<T>& list, pugi::xml_node element);
	/** List<T> stored by pointer (storage_by_pointer) */
	template <XmlObject T>
	void bindList(std::vector<std::unique_ptr<T>>& list, pugi::xml_node element);
	/** @XmlElements list: returns false (and consumes nothing) if `name` is not one of the factory's element names */
	template <class Base>
	bool bindChoice(std::vector<std::unique_ptr<Base>>& list, const ElementFactory<Base>& factory, pugi::xml_node element, std::string_view name);
	/** @XmlElements single property (repeated: like bindSingle) */
	template <class Base>
	bool bindChoice(std::unique_ptr<Base>& slot, const ElementFactory<Base>& factory, pugi::xml_node element, std::string_view name);
	/**
	 * @XmlElementWrapper(name) + @XmlElement(itemName): engages the optional (present-empty gives an empty list), reserves, binds every
	 * `itemName` child with bindList. A repeated wrapper is an error in strict mode; otherwise it replaces the list (JAXB's Lister clears the
	 * collection) and the replaced elements are kept alive by LoadContext::retire. Other children, attributes and text of the wrapper are
	 * unknown. Children's hooks get the object that declares the wrapper as parent.
	 */
	template <class L>
	void bindWrapper(std::optional<L>& list, pugi::xml_node wrapper, std::string_view itemName);
	/** Text content of an element without attributes or child elements (concatenated PCDATA/CDATA); counts the element as bound. */
	std::string elementText(pugi::xml_node element);
	/** @XmlElement scalar/String into T or std::optional<T> */
	template <XmlScalarTarget T>
	void bindText(T& target, pugi::xml_node element);
	/** @XmlList @XmlElement */
	template <XmlListTarget C>
	void bindTextList(C& target, pugi::xml_node element);
	/** adapter on element text */
	template <class F>
	auto adaptText(F&& adapter, pugi::xml_node element) -> decltype(std::forward<F>(adapter)(std::string_view{}));
	/** consumes an element deliberately without binding it (counted as ignored with all descendants and their attributes) */
	void ignoreElement(pugi::xml_node element);

	// ---- XmlID / XmlIDREF --------------------------------------------------------------------------------------------------------------------------
	template <class T>
	void registerXmlId(std::string_view id, const T& object) {
		loadContext.registerXmlId(id, object);
	}
	/** @XmlIDREF attribute */
	template <class T>
	void idRef(const T*& slot, std::string_view id) {
		loadContext.addIdRef(slot, trimXml(id));
	}
	/** @XmlIDREF attribute into a list element (appends) */
	template <class T>
	void idRef(std::vector<const T*>& list, std::string_view id) {
		loadContext.addIdRef(list, trimXml(id));
	}
	/** @XmlIDREF @XmlList attribute: one reference per whitespace-separated token (appends) */
	template <class T>
	void idRefList(std::vector<const T*>& list, std::string_view ids) {
		for (std::string_view id : splitXmlList(ids))
			loadContext.addIdRef(list, id);
	}
	/** @XmlIDREF element (text content) */
	template <class T>
	void idRefText(const T*& slot, pugi::xml_node element) {
		std::string text = elementText(element);
		positionAt(element);
		loadContext.addIdRef(slot, trimXml(text));
	}
	/** @XmlIDREF element list (text content, appends) */
	template <class T>
	void idRefText(std::vector<const T*>& list, pugi::xml_node element) {
		std::string text = elementText(element);
		positionAt(element);
		loadContext.addIdRef(list, trimXml(text));
	}

	// ---- required (inside XmlBinding<T>::finish) -------------------------------------------------------------------------------------------------
	/** @XmlAttribute(required = true): each name must be present on the current element. @throws StaticDataException */
	void checkRequiredAttributes(std::initializer_list<std::string_view> names) const;
	/** @XmlElement(required = true): each name must occur among the children (of every file of a holder). @throws StaticDataException */
	void checkRequiredElements(std::initializer_list<std::string_view> names) const;

	// ---- diagnostics -------------------------------------------------------------------------------------------------------------------------------
	/** @throws StaticDataException `location: path (Class): message` at the attribute or element being bound */
	[[noreturn]] void fail(std::string_view message) const;
	/** @throws StaticDataException at `node` */
	[[noreturn]] void failAt(pugi::xml_node node, std::string_view message) const;
	/** logs once per key (LoadContext::warnOnce) with the current location */
	void warnOnce(std::string_view key, std::string_view message);
	/** "file:line:col: path (Class)" of the attribute or element being bound (empty outside binding) */
	std::string describeCurrent() const;
	/** the element being bound (empty outside binding) */
	pugi::xml_node currentNode() const noexcept;
	/** Java simple name of the class being bound */
	std::string_view currentClassName() const noexcept;
	/** the object being bound, i.e. the parent of the children bound now */
	XmlParent currentObject() const noexcept;
	/** file index (LoadContext::internFile) and location of the attribute or element being bound */
	std::pair<uint32_t, XmlLocation> currentPosition() const noexcept;

private:
	struct Frame {
		pugi::xml_node node;
		const XmlDocument* document;
		uint32_t file;
		std::string_view className;
		XmlParent object;
		/** all documents of a holder frame (required element checks); empty otherwise */
		std::span<const XmlDocument* const> documents;
	};
	class FrameGuard {
	public:
		FrameGuard(BindContext& context, Frame frame);
		~FrameGuard();
		FrameGuard(const FrameGuard&) = delete;
		FrameGuard& operator=(const FrameGuard&) = delete;

	private:
		BindContext& context;
		pugi::xml_attribute savedAttribute;
		pugi::xml_node savedPosition;
	};

	template <class T>
	void bindAttributes(T& object, pugi::xml_node node);
	template <class T>
	void bindChildren(T& object, pugi::xml_node node);
	template <class T>
	void bindObjectIn(T& object, pugi::xml_node node, XmlParent parent, const XmlDocument& document, uint32_t file);
	template <class F>
	decltype(auto) convert(F&& function);

	Frame& top();
	const Frame* topOrNull() const noexcept;
	/** counts an element as consumed (and bound) */
	void countElementBound(pugi::xml_node element);
	void countAttributeBound(pugi::xml_node element, pugi::xml_attribute attribute);
	/** true for xmlns declarations and xsi attributes (counted as ignored by the caller) */
	bool isIgnoredAttribute(pugi::xml_node element, std::string_view name) const;
	void handleAttribute(pugi::xml_node element, pugi::xml_attribute attribute, bool bound);
	/** counts an attribute for which isIgnoredAttribute returned true (namespace declaration or namespaced attribute) */
	void countNamespaceAttribute(std::string_view name);
	void unknownAttribute(pugi::xml_node element, pugi::xml_attribute attribute);
	[[noreturn]] void unknownElement(pugi::xml_node element);
	void unexpectedText(pugi::xml_node text);
	[[noreturn]] void notCounted(pugi::xml_node element);
	void checkListCapacity(size_t size, size_t capacity, pugi::xml_node element);
	void checkRepeated(bool alreadySet, pugi::xml_node element);
	void warnEmptyString();
	/** location override for IDREFs of text elements, until the next attribute/element */
	void positionAt(pugi::xml_node node) noexcept { positionNode = node; }
	void mergeRootTag(pugi::xml_node first, pugi::xml_node root);
	std::string pathString() const;

	LoadContext& loadContext;
	BindContext* previousBinding;
	LoadContext::Checkpoint checkpoint;
	int uncaughtExceptions;
	std::vector<Frame> frames;
	pugi::xml_attribute currentAttribute;
	pugi::xml_node positionNode;
	/** number of elements consumed so far (checks that a binder that returned true consumed the element) */
	uint64_t consumed = 0;
	/** true while XmlBinding<T>::attribute runs (ignoreAttribute) */
	bool bindingAttribute = false;
	/** set by ignoreAttribute for the attribute being bound */
	bool attributeIgnoredByBinder = false;
	bool collectStats;
};

// =============================================================================================================================================
// template implementation
// =============================================================================================================================================

template <class F>
decltype(auto) BindContext::convert(F&& function) {
	try {
		return std::forward<F>(function)();
	} catch (const commons::utils::IllegalArgumentException& e) {
		fail(e.what());
	}
}

template <class T>
std::unique_ptr<T> BindContext::bindRoot(const XmlDocument& document, const XmlParent& parent) {
	static_assert(CompleteXmlBinding<T>, "XmlBinding<T> is not declared in this translation unit (include the generated .bind.h)");
	std::unique_ptr<T> object = constructBound<T>();
	bindObjectIn(*object, document.root(), parent, document, loadContext.internFile(document.displayName()));
	return object;
}

template <class T>
void BindContext::bindObject(T& object, pugi::xml_node node, const XmlParent& parent) {
	if (frames.empty())
		throw commons::utils::IllegalStateException("BindContext::bindObject called outside bindRoot/bindHolder");
	bindObjectIn(object, node, parent, *frames.back().document, frames.back().file);
}

template <class T>
void BindContext::bindObjectIn(T& object, pugi::xml_node node, XmlParent parent, const XmlDocument& document, uint32_t file) {
	static_assert(CompleteXmlBinding<T>, "XmlBinding<T> is not declared in this translation unit (include the generated .bind.h)");
	FrameGuard guard(*this, Frame{node, &document, file, bindingClassName<T>(), XmlParent::of(object, node.name()), {}});
	countElementBound(node);
	if constexpr (BindingHasReserve<T>) {
		ChildCounts counts;
		counts.addChildren(node);
		XmlBinding<T>::reserve(object, counts);
	}
	bindAttributes(object, node);
	bindChildren(object, node);
	if constexpr (BindingHasFinish<T>)
		XmlBinding<T>::finish(object, *this, parent);
}

template <class H>
void BindContext::bindHolder(H& holder, std::span<const XmlDocument* const> documents, const XmlParent& parent) {
	static_assert(CompleteXmlBinding<H>, "XmlBinding<H> is not declared in this translation unit (include the generated .bind.h)");
	if (documents.empty())
		throw commons::utils::IllegalArgumentException("bindHolder needs at least one document");
	pugi::xml_node first = documents.front()->root();
	uint32_t firstFile = loadContext.internFile(documents.front()->displayName());
	FrameGuard guard(*this, Frame{first, documents.front(), firstFile, bindingClassName<H>(), XmlParent::of(holder, first.name()), documents});
	countElementBound(first); // the roots of later files are dropped by the merge (mergeRootTag counts them as skipped roots)
	if constexpr (BindingHasReserve<H>) {
		ChildCounts counts;
		for (const XmlDocument* document : documents)
			counts.addChildren(document->root());
		XmlBinding<H>::reserve(holder, counts);
	}
	bindAttributes(holder, first);
	for (size_t i = 0; i < documents.size(); ++i) {
		const XmlDocument& document = *documents[i];
		pugi::xml_node root = document.root();
		top().node = root;
		top().document = &document;
		top().file = loadContext.internFile(document.displayName());
		if (i > 0)
			mergeRootTag(first, root);
		bindChildren(holder, root);
	}
	top().node = first;
	top().document = documents.front();
	top().file = firstFile;
	if constexpr (BindingHasFinish<H>)
		XmlBinding<H>::finish(holder, *this, parent);
}

template <class T>
void BindContext::bindAttributes(T& object, pugi::xml_node node) {
	for (pugi::xml_attribute attribute = node.first_attribute(); attribute; attribute = attribute.next_attribute()) {
		std::string_view name = attribute.name();
		currentAttribute = attribute;
		bool bound = false;
		if (!isIgnoredAttribute(node, name)) {
			if constexpr (BindingHasAttribute<T>) {
				attributeIgnoredByBinder = false;
				bindingAttribute = true;
				try {
					bound = XmlBinding<T>::attribute(object, *this, name, std::string_view(attribute.value()));
				} catch (...) {
					bindingAttribute = false;
					throw;
				}
				bindingAttribute = false;
			}
			handleAttribute(node, attribute, bound);
		} else {
			countNamespaceAttribute(name);
		}
		currentAttribute = pugi::xml_attribute();
	}
}

template <class T>
void BindContext::bindChildren(T& object, pugi::xml_node node) {
	for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
		switch (child.type()) {
			case pugi::node_element: {
				uint64_t before = consumed;
				positionNode = pugi::xml_node();
				bool handled = false;
				if constexpr (BindingHasElement<T>)
					handled = XmlBinding<T>::element(object, *this, child, std::string_view(child.name()));
				positionNode = pugi::xml_node();
				if (!handled)
					unknownElement(child);
				if (consumed == before)
					notCounted(child);
				break;
			}
			case pugi::node_pcdata:
			case pugi::node_cdata:
				unexpectedText(child);
				break;
			default:
				break;
		}
	}
}

template <XmlObject T>
void BindContext::bindSingle(std::unique_ptr<T>& slot, pugi::xml_node element) {
	checkRepeated(slot != nullptr, element);
	std::unique_ptr<T> object = constructBound<T>();
	bindObject(*object, element, currentObject());
	loadContext.retire(std::move(slot)); // lenient repeat: XmlIDs, IDREF slots and tasks of the replaced object may point into it
	slot = std::move(object);
}

template <class T>
void BindContext::bindList(std::vector<T>& list, pugi::xml_node element) {
	if constexpr (XmlScalar<T>) {
		std::string text = elementText(element);
		FrameGuard guard(*this, Frame{element, top().document, top().file, top().className, top().object, {}});
		list.push_back(value<T>(text));
	} else {
		static_assert(XmlObject<T>, "bindList: element type must be a scalar or a bound class");
		checkListCapacity(list.size(), list.capacity(), element);
		T& item = list.emplace_back();
		bindObject(item, element, currentObject());
	}
}

template <XmlObject T>
void BindContext::bindList(std::vector<std::unique_ptr<T>>& list, pugi::xml_node element) {
	std::unique_ptr<T> object = constructBound<T>();
	bindObject(*object, element, currentObject());
	list.push_back(std::move(object));
}

template <class Base, class Derived>
std::unique_ptr<Base> bindChoiceAs(BindContext& context, pugi::xml_node element, const XmlParent& parent) {
	static_assert(std::derived_from<Derived, Base>, "@XmlElements choice: Derived must derive from Base");
	static_assert(std::same_as<Base, Derived> || std::has_virtual_destructor_v<Base>,
	              "@XmlElements choice: Base needs a virtual destructor (choice objects are owned and deleted as std::unique_ptr<Base>)");
	std::unique_ptr<Derived> object = constructBound<Derived>();
	context.bindObject(*object, element, parent);
	return object;
}

template <class Base>
bool BindContext::bindChoice(std::vector<std::unique_ptr<Base>>& list, const ElementFactory<Base>& factory, pugi::xml_node element,
                             std::string_view name) {
	const ChoiceEntry<Base>* entry = factory.find(name);
	if (entry == nullptr)
		return false;
	list.push_back(entry->bind(*this, element, currentObject()));
	return true;
}

template <class Base>
bool BindContext::bindChoice(std::unique_ptr<Base>& slot, const ElementFactory<Base>& factory, pugi::xml_node element, std::string_view name) {
	const ChoiceEntry<Base>* entry = factory.find(name);
	if (entry == nullptr)
		return false;
	checkRepeated(slot != nullptr, element);
	std::unique_ptr<Base> object = entry->bind(*this, element, currentObject());
	loadContext.retire(std::move(slot)); // see bindSingle
	slot = std::move(object);
	return true;
}

template <class L>
void BindContext::bindWrapper(std::optional<L>& list, pugi::xml_node wrapper, std::string_view itemName) {
	bool repeated = list.has_value();
	checkRepeated(repeated, wrapper);
	FrameGuard guard(*this, Frame{wrapper, top().document, top().file, top().className, top().object, {}});
	countElementBound(wrapper);
	for (pugi::xml_attribute attribute = wrapper.first_attribute(); attribute; attribute = attribute.next_attribute()) {
		currentAttribute = attribute;
		if (isIgnoredAttribute(wrapper, attribute.name()))
			countNamespaceAttribute(attribute.name());
		else
			handleAttribute(wrapper, attribute, false);
		currentAttribute = pugi::xml_attribute();
	}
	if (repeated) // lenient: moving the vector keeps its elements in place, so their XmlIDs, IDREF slots and tasks stay valid
		loadContext.retire(std::make_unique<L>(std::move(*list)));
	list.emplace();
	size_t count = 0;
	for (pugi::xml_node child = wrapper.first_child(); child; child = child.next_sibling()) {
		if (child.type() == pugi::node_element && itemName == child.name())
			++count;
	}
	list->reserve(count);
	for (pugi::xml_node child = wrapper.first_child(); child; child = child.next_sibling()) {
		switch (child.type()) {
			case pugi::node_element:
				if (itemName != child.name())
					unknownElement(child);
				bindList(*list, child);
				break;
			case pugi::node_pcdata:
			case pugi::node_cdata:
				unexpectedText(child);
				break;
			default:
				break;
		}
	}
}

template <XmlScalarTarget T>
void BindContext::bindText(T& target, pugi::xml_node element) {
	std::string text = elementText(element);
	FrameGuard guard(*this, Frame{element, top().document, top().file, top().className, top().object, {}});
	if constexpr (Optional<T>) {
		target = value<typename T::value_type>(text);
	} else {
		if constexpr (std::same_as<T, std::string>) {
			if (text.empty())
				warnEmptyString();
		}
		target = value<T>(text);
	}
}

template <XmlListTarget C>
void BindContext::bindTextList(C& target, pugi::xml_node element) {
	std::string text = elementText(element);
	FrameGuard guard(*this, Frame{element, top().document, top().file, top().className, top().object, {}});
	target = convert([&] { return parseList<C>(text); });
}

template <class F>
auto BindContext::adaptText(F&& adapter, pugi::xml_node element) -> decltype(std::forward<F>(adapter)(std::string_view{})) {
	std::string text = elementText(element);
	FrameGuard guard(*this, Frame{element, top().document, top().file, top().className, top().object, {}});
	return convert([&]() -> decltype(auto) { return std::forward<F>(adapter)(std::string_view(text)); });
}

} // namespace aion::gameserver::xml
