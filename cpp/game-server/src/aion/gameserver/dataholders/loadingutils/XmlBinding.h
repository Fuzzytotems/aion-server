#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"
#include "aion/gameserver/dataholders/loadingutils/XmlParent.h"

/**
 * ============================================================================================================================================
 * Static data XML binder runtime: the contract for generated code (tools/xmlgen, docs/design/static-data.md §2-§3)
 * ============================================================================================================================================
 *
 * Replaces JAXB unmarshalling. For every bound Java class X, generated code specializes `aion::gameserver::xml::XmlBinding<X>`; the
 * hand-written runtime (BindContext) walks the pugixml DOM and calls it. Namespace of everything here: aion::gameserver::xml.
 *
 * Headers
 * -------
 * - Game headers (hand-written behaviour classes, generated data-only headers) include only XmlBindingFwd.h (and EnumTraits.h for enums).
 * - Generated binder code includes "aion/gameserver/dataholders/loadingutils/BindContext.h" (it includes this header, ElementFactory.h,
 *   LoadContext.h and the converters) plus the class headers it binds. Binder headers are never included by game code.
 * - Explicit specializations must be declared before the first use in every TU that binds the type: generate one declaration header per Java
 *   package (e.g. `model/templates/item/item.bind.h` declaring the XmlBinding<X> specializations with their static member declarations)
 *   and one `item.bind.cpp` with the definitions. Using a type whose specialization is not declared is a compile error (static_assert in
 *   BindContext), never a silent "binds nothing".
 *
 * Shape of a specialization (every member is optional; the runtime detects each one with the concepts below)
 * ------------------------------------------------------------------------------------------------------------
 * <pre>
 * namespace aion::gameserver::xml {
 * template <>
 * struct XmlBinding<model::templates::item::ItemTemplate> {
 * 	using T = model::templates::item::ItemTemplate;
 * 	static constexpr std::string_view CLASS_NAME = "ItemTemplate";              // Java simple name (messages, warn-once keys)
 *
 * 	// One attribute of the element. `value` is the attribute value after XML attribute normalization. Return true if the attribute belongs
 * 	// to this class (the runtime counts it as bound), false otherwise (unknown: StaticDataException in strict mode, warning otherwise).
 * 	// Chain to the bound (or empty) base binding at the end: `return XmlBinding<Base>::attribute(o, c, name, value);`
 * 	static bool attribute(T& o, BindContext& c, std::string_view name, std::string_view value);
 *
 * 	// One child element. Return true if consumed; a consumed element must go through exactly one BindContext child helper (bindSingle,
 * 	// bindList, bindChoice, bindWrapper, bindText, bindTextList, adaptText, idRefText, elementText, ignoreElement, or bindObject for
 * 	// hand-written code), otherwise the runtime throws IllegalStateException (binder bug). Return false for an unknown element: always a
 * 	// StaticDataException (JAXB's XmlValidationHandler also fails on unexpected elements). Chain to the base binding like attribute().
 * 	static bool element(T& o, BindContext& c, pugi::xml_node e, std::string_view name);
 *
 * 	// Called before attributes and children with the element-name counts of all children (for a holder: summed over every file of the
 * 	// import). Must reserve every std::vector<U> of objects bound in place: `o.items.reserve(counts["item"]);` (bindList refuses to grow a
 * 	// vector past its capacity, so element addresses never change and XmlIDs/hooks may keep pointers). Chain to the base: its vectors too.
 * 	static void reserve(T& o, const ChildCounts& counts);
 *
 * 	// After attributes and children: required checks, then the hook (JAXB post-order: children's hooks ran already).
 * 	static void finish(T& o, BindContext& c, const XmlParent& parent);
 *
 * 	// Only for classes without an accessible default constructor or with a custom allocation: used by bindSingle, unique_ptr lists and
 * 	// choice factories instead of std::make_unique<T>().
 * 	static std::unique_ptr<T> create();
 * };
 * } // namespace aion::gameserver::xml
 * </pre>
 *
 * Typical generated bodies (all BindContext calls attach file:line:col and the element path to conversion errors):
 * <pre>
 * bool XmlBinding<ItemTemplate>::attribute(ItemTemplate& o, BindContext& c, std::string_view n, std::string_view v) {
 * 	switch (nameHash(n)) {
 * 	case "mask"_xh:     if (n == "mask") { c.assign(o.mask, v); return true; } break;                         // int32_t mask = 0
 * 	case "quality"_xh:  if (n == "quality") { c.assign(o.itemQuality, v); return true; } break;              // std::optional<ItemQuality>
 * 	case "id"_xh:       if (n == "id") { o.setXmlUid(v); c.registerXmlId(v, o); return true; } break;        // @XmlID @XmlAttribute method
 * 	case "restrict"_xh: if (n == "restrict") { o.levelRestrictions = adapters::parseSpaceSeparatedBytes(c, v); return true; } break;
 * 	case "pre_effects"_xh: if (n == "pre_effects") { c.assignList(o.preEffects, v); return true; } break;    // std::optional<std::vector<int32_t>>
 * 	}
 * 	return XmlBinding<VisibleObjectTemplate>::attribute(o, c, n, v);
 * }
 * bool XmlBinding<ItemTemplate>::element(ItemTemplate& o, BindContext& c, pugi::xml_node e, std::string_view n) {
 * 	switch (nameHash(n)) {
 * 	case "actions"_xh:  if (n == "actions") { c.bindSingle(o.actions, e); return true; } break;             // std::unique_ptr<ItemActions>
 * 	case "stat"_xh:     if (n == "stat") { c.bindList(o.stats, e); return true; } break;                      // std::vector<Stat>, reserved
 * 	case "desc"_xh:     if (n == "desc") { c.bindText(o.description, e); return true; } break;                // @XmlElement String
 * 	case "config_properties"_xh: if (n == "config_properties") { c.bindWrapper(o.configProperties, e, "property"); return true; } break;
 * 	}
 * 	if (c.bindChoice(o.effects, EFFECTS_FACTORY, e, n)) return true;                                          // @XmlElements list
 * 	return XmlBinding<VisibleObjectTemplate>::element(o, c, e, n);
 * }
 * void XmlBinding<ItemTemplate>::reserve(ItemTemplate& o, const ChildCounts& counts) {
 * 	o.stats.reserve(counts["stat"]);
 * 	XmlBinding<VisibleObjectTemplate>::reserve(o, counts);
 * }
 * void XmlBinding<ItemTemplate>::finish(ItemTemplate& o, BindContext& c, const XmlParent& p) {
 * 	c.checkRequiredAttributes({"id", "level"});                          // flattened over the class hierarchy
 * 	c.checkRequiredElements({"actions"});
 * 	if (c.hooksEnabled())
 * 		o.afterUnmarshal(c.load(), p);                                   // the nearest declared Java hook, called once
 * }
 * </pre>
 *
 * Member kinds -> BindContext calls
 * ---------------------------------
 * - scalar @XmlAttribute (int, float, boolean, enum, ...) into T or std::optional<T>: `c.assign(member, v)`
 * - String @XmlAttribute into std::string or std::optional<std::string>: `c.assign(member, v)` (an empty value into a non-optional string
 *   warns once per class and attribute)
 * - collection-typed attribute or @XmlList attribute into [std::optional<]std::vector<T> or std::unordered_set<T>[>]: `c.assignList(member, v)`
 * - @XmlJavaTypeAdapter attribute: `member = adapter(c, v)` or `member = c.adapt(fn, v)`
 * - @XmlElement scalar or String (text content) into T, std::optional<T> or std::string: `c.bindText(member, e)`
 * - @XmlList @XmlElement into [std::optional<]std::vector<T>[>]: `c.bindTextList(member, e)`
 * - @XmlElement object into std::unique_ptr<T>: `c.bindSingle(member, e)`
 * - List<T> / T[] element list into std::vector<T> (objects: reserved) or std::vector<std::unique_ptr<T>>: `c.bindList(member, e)`
 * - @XmlElements list or single property into std::vector<std::unique_ptr<Base>> or std::unique_ptr<Base>:
 *   `c.bindChoice(member, FACTORY, e, n)` (returns false for names that are not choices)
 * - @XmlElementWrapper + @XmlElement into std::optional<std::vector<T or std::unique_ptr<T>>>: `c.bindWrapper(member, e, "itemName")`
 * - @XmlID (attribute or setter): `c.registerXmlId(v, o)` after assigning
 * - @XmlIDREF into const T* or std::vector<const T*>: attribute `c.idRef(member, v)`, element text `c.idRefText(member, e)`,
 *   @XmlList attribute `c.idRefList(member, v)`
 * - deliberately unbound element (xmlgen.toml deny_implicit, ...): `c.ignoreElement(e)`
 * - deliberately unbound attribute (xmlgen.toml [ignore_attributes]): `static_cast<void>(value); c.ignoreAttribute(); return true;` (counted
 *   as ignored, not bound, in BindStats)
 *
 * Repeated single elements and wrappers are an error in strict mode (XSD maxOccurs=1). Lenient mode does what JAXB does: the last single
 * element wins, a repeated wrapper replaces the list (JAXB's Lister clears the collection when a wrapper starts); the replaced objects are kept
 * alive by LoadContext::retire because XmlIDs, IDREF slots and hook tasks may point into them. Elements of a plain list accumulate in
 * document order even when other elements are interleaved.
 *
 * @XmlElements factories (ElementFactory.h)
 * ----------------------------------------
 * <pre>
 * inline constexpr ChoiceEntry<EffectTemplate> EFFECTS_CHOICES[] = {        // sorted by element name (byte order), unique
 * 	choice<EffectTemplate, AbsoluteEXPPointHealInstantEffect>("absexppointhealinstant"), ...};
 * inline constexpr ElementFactory<EffectTemplate> EFFECTS_FACTORY{EFFECTS_CHOICES};
 * static_assert(isStrictlySortedByName(EFFECTS_CHOICES));
 * </pre>
 * A choice binds the concrete class with XmlBinding<Derived> (whose attribute/element chain to the bases), so hooks see the concrete type and
 * XmlParent::as<Derived>() works for its children. The lists own the objects as std::unique_ptr<Base>, so Base must declare a virtual
 * destructor (static_assert in choice() and bindChoiceAs).
 *
 * Classes with behaviour (`#include "X.xml.inc"` inside the hand-written class body) and data-only structs
 * -----------------------------------------------------------------------------------------------------
 * Both get the same XmlBinding<X> specialization; they only differ in where the members are declared. The generated member block starts with
 * `friend struct ::aion::gameserver::xml::XmlBinding<::full::X>;` so the binder can write private members and call the private hook and
 * @XmlID/@XmlAttribute setters. A data-only struct has public members and no hook. Holder classes are ordinary bound classes; the loader
 * binds them through HolderRegistration::of<H>(rootTag) (StaticDataLoader.h).
 *
 * Hooks
 * -----
 * Java `void afterUnmarshal(Unmarshaller u, Object parent)` becomes `void afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& parent)`,
 * called from the generated finish() when c.hooksEnabled(). LoadContext gives holder<H>() (earlier holders of the load, declared dependencies
 * only), runAfterUnmarshalTask (inline), runAfterIdRefResolution, fail/warn with the current location. beforeUnmarshal is not supported (the
 * only Java use installs the StaticDataListener, which LoadContext replaces).
 *
 * Class-level adapters (NpcEquippedGearAdapter)
 * ---------------------------------------------
 * Bind the adapter's value type on the heap with bindObject (it counts the element and runs the value type's binding), hand ownership to the
 * hand-written target and let it finish after IDREF resolution. IDREF slots and XmlIDs record addresses, so a bound object must never be
 * moved or copied afterwards (a moved std::vector keeps its buffer, but the recorded vector object would dangle). Store the target with
 * replaceSingle, never with a plain assignment: a repeated element must not destroy the previous target, whose IDREF slots and tasks are
 * registered (error in strict mode, retired in lenient mode):
 * <pre>
 * if (n == "equipment") {
 * 	auto list = std::make_unique<NpcEquipmentList>();
 * 	c.bindObject(*list, e, c.currentObject());
 * 	c.replaceSingle(o.equipment, NpcEquippedGear::create(std::move(list)), e);   // Ref<NpcEquippedGear>, keeps the list
 * 	o.equipment->init(c.load());                                            // runAfterIdRefResolution(...) inside
 * 	return true;
 * }
 * </pre>
 *
 * Failures
 * --------
 * Everything throws StaticDataException with `file:line:col: element/path@attribute (Class): message`, except binder bugs (a consumed element
 * without a helper call, an unreserved in-place list), which throw IllegalStateException. When a BindContext is left by an exception, the
 * XmlIDs, IDREF slots and after-IDREF tasks registered through it are rolled back, so the LoadContext never points into destroyed objects.
 *
 * Name switches
 * -------------
 * nameHash / "..."_xh give a 64-bit FNV-1a hash for `switch`; always compare the full name inside the case (hash collisions between names
 * of one switch are a compile error because of duplicate case labels; the generator then falls back to an if chain).
 */
namespace aion::gameserver::xml {

/** 64-bit FNV-1a of a name, for switch statements in generated binders. */
constexpr uint64_t nameHash(std::string_view name) noexcept {
	uint64_t hash = 0xcbf29ce484222325ull;
	for (char c : name) {
		hash ^= static_cast<uint8_t>(c);
		hash *= 0x100000001b3ull;
	}
	return hash;
}

inline namespace literals {
/** `case "mask"_xh:` */
constexpr uint64_t operator""_xh(const char* name, size_t length) noexcept {
	return nameHash(std::string_view(name, length));
}
} // namespace literals

/** Element-name counts of the children of an element (or of all roots of a directory import), passed to XmlBinding<T>::reserve. */
class ChildCounts {
public:
	/** number of child elements with this name (0 if none) */
	size_t operator[](std::string_view name) const noexcept;
	/** number of child elements */
	size_t total() const noexcept { return totalCount; }
	/** adds the element children of `parent` */
	void addChildren(pugi::xml_node parent);

private:
	std::vector<std::pair<std::string_view, size_t>> counts;
	size_t totalCount = 0;
};

/** true if XmlBinding<T> is a declared (complete) specialization in this TU */
template <class T>
concept CompleteXmlBinding = requires { sizeof(XmlBinding<T>); };

template <class T>
concept BindingHasClassName = requires {
	{ XmlBinding<T>::CLASS_NAME } -> std::convertible_to<std::string_view>;
};

template <class T>
concept BindingHasAttribute = requires(T& object, BindContext& context, std::string_view name, std::string_view value) {
	{ XmlBinding<T>::attribute(object, context, name, value) } -> std::same_as<bool>;
};

template <class T>
concept BindingHasElement = requires(T& object, BindContext& context, pugi::xml_node element, std::string_view name) {
	{ XmlBinding<T>::element(object, context, element, name) } -> std::same_as<bool>;
};

template <class T>
concept BindingHasReserve = requires(T& object, const ChildCounts& counts) { XmlBinding<T>::reserve(object, counts); };

template <class T>
concept BindingHasFinish = requires(T& object, BindContext& context, const XmlParent& parent) { XmlBinding<T>::finish(object, context, parent); };

template <class T>
concept BindingHasCreate = requires {
	{ XmlBinding<T>::create() } -> std::convertible_to<std::unique_ptr<T>>;
};

/** Java simple class name of a bound type (XmlBinding<T>::CLASS_NAME, otherwise the C++ type name). */
template <class T>
std::string_view bindingClassName() noexcept {
	if constexpr (BindingHasClassName<T>)
		return XmlBinding<T>::CLASS_NAME;
	else
		return typeid(T).name();
}

/** Creates an object to bind: XmlBinding<T>::create() if declared, otherwise std::make_unique<T>(). */
template <class T>
std::unique_ptr<T> constructBound() {
	if constexpr (BindingHasCreate<T>)
		return XmlBinding<T>::create();
	else
		return std::make_unique<T>();
}

} // namespace aion::gameserver::xml
