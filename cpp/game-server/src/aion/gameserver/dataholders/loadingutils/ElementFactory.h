#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

#include <pugixml.hpp>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"
#include "aion/gameserver/dataholders/loadingutils/XmlParent.h"

namespace aion::gameserver::xml {

/** One element name of an @XmlElements list and the function that creates and binds its class. */
template <class Base>
struct ChoiceEntry {
	std::string_view name;
	std::unique_ptr<Base> (*bind)(BindContext& context, pugi::xml_node element, const XmlParent& parent);
};

/** Creates a Derived, binds it with XmlBinding<Derived> and returns it as Base (defined in BindContext.h). */
template <class Base, class Derived>
std::unique_ptr<Base> bindChoiceAs(BindContext& context, pugi::xml_node element, const XmlParent& parent);

/**
 * Table entry for `<name>` -> Derived: `choice<EffectTemplate, BufEffect>("buf")`. Base must have a virtual destructor unless Derived is Base:
 * choice objects are owned as std::unique_ptr<Base> (the hand-written behaviour bases declare `virtual ~Base() = default;`).
 */
template <class Base, class Derived>
constexpr ChoiceEntry<Base> choice(std::string_view name) noexcept {
	static_assert(std::derived_from<Derived, Base>, "@XmlElements choice: Derived must derive from Base");
	static_assert(std::same_as<Base, Derived> || std::has_virtual_destructor_v<Base>,
	              "@XmlElements choice: Base needs a virtual destructor (choice objects are owned and deleted as std::unique_ptr<Base>)");
	return ChoiceEntry<Base>{name, &bindChoiceAs<Base, Derived>};
}

/**
 * Element-name factory of one @XmlElements property (docs/design/static-data.md §2.2): a view of a constexpr table sorted by element name
 * (byte order, case-sensitive, unique; check with `static_assert(isStrictlySortedByName(TABLE))`). Lookup is a binary search. The table must
 * have static storage duration.
 */
template <class Base>
class ElementFactory {
public:
	constexpr explicit ElementFactory(std::span<const ChoiceEntry<Base>> entries) noexcept : table(entries) {}

	/** the entry for an element name, or nullptr */
	constexpr const ChoiceEntry<Base>* find(std::string_view name) const noexcept {
		size_t low = 0;
		size_t high = table.size();
		while (low < high) {
			size_t mid = low + (high - low) / 2;
			std::string_view candidate = table[mid].name;
			if (candidate < name)
				low = mid + 1;
			else if (name < candidate)
				high = mid;
			else
				return &table[mid];
		}
		return nullptr;
	}

	constexpr std::span<const ChoiceEntry<Base>> entries() const noexcept { return table; }

private:
	std::span<const ChoiceEntry<Base>> table;
};

} // namespace aion::gameserver::xml
