#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"

namespace aion::gameserver::xml {

/** One entry of an enum's XML lookup table: lexical XML form (the @XmlEnumValue, otherwise the constant name) and the constant. */
template <class E>
struct EnumEntry {
	std::string_view name;
	E value;
};

/**
 * Name tables of a generated enum (docs/design/static-data.md §2.5). They replace magic_enum for the JAXB enums (TribeClass has 724
 * constants, far outside magic_enum's range).
 *
 * Shape of the specialization emitted by the generator next to the enum (`enum class E : <underlying> { A, B, ... }`, constants in Java
 * ordinal order with implicit values 0..N-1):
 *
 * <pre>
 * template <>
 * struct xml::EnumTraits<ItemGroup> {
 * 	static constexpr std::string_view javaName = "ItemGroup";                               // Java simple class name (messages)
 * 	static constexpr std::array<std::string_view, 3> names{"NONE", "NOWEAPON", "SWORD"};     // Java name(), ordinal order
 * 	static constexpr std::array<EnumEntry<ItemGroup>, 3> xmlSorted{{                          // XML lexical form, sorted by byte order
 * 		{"NONE", ItemGroup::NONE}, {"NOWEAPON", ItemGroup::NOWEAPON}, {"SWORD", ItemGroup::SWORD}}};
 * };
 * static_assert(xml::verifyEnumTraits<ItemGroup>());
 * </pre>
 *
 * `xmlSorted` uses the @XmlEnumValue string where one exists (ZoneAttributes), otherwise the constant name. The generator sorts it (Python
 * `sorted` on UTF-8 bytes equals std::string_view ordering); verifyEnumTraits checks order, uniqueness and ordinal coverage at compile time.
 * The specialization must live in namespace aion::gameserver::xml (write `namespace aion::gameserver::xml { template <> struct ... }`).
 */
template <class E>
concept XmlEnum = std::is_enum_v<E> && requires {
	{ EnumTraits<E>::javaName } -> std::convertible_to<std::string_view>;
	{ EnumTraits<E>::names.size() } -> std::convertible_to<size_t>;
	{ EnumTraits<E>::xmlSorted.size() } -> std::convertible_to<size_t>;
};

/** true if the names of `entries` are in strictly increasing byte order (sorted and unique); usable in static_assert. */
template <class Range>
constexpr bool isStrictlySortedByName(const Range& entries) noexcept {
	auto it = std::begin(entries);
	auto end = std::end(entries);
	if (it == end)
		return true;
	for (auto next = std::next(it); next != end; it = next, ++next) {
		if (!(std::string_view(it->name) < std::string_view(next->name)))
			return false;
	}
	return true;
}

/** Java ordinal() of a generated enum constant (the underlying value). */
template <XmlEnum E>
constexpr int32_t enumOrdinal(E value) noexcept {
	return static_cast<int32_t>(std::to_underlying(value));
}

/** Checks a generated EnumTraits specialization: same table sizes, `xmlSorted` strictly sorted, every ordinal exactly once. */
template <XmlEnum E>
constexpr bool verifyEnumTraits() noexcept {
	const auto& names = EnumTraits<E>::names;
	const auto& sorted = EnumTraits<E>::xmlSorted;
	if (names.size() != sorted.size() || !isStrictlySortedByName(sorted))
		return false;
	for (size_t ordinal = 0; ordinal < names.size(); ++ordinal) {
		size_t occurrences = 0;
		for (const auto& entry : sorted) {
			if (static_cast<size_t>(enumOrdinal(entry.value)) == ordinal)
				++occurrences;
		}
		if (occurrences != 1)
			return false;
	}
	return true;
}

/** Looks up the constant for an XML lexical value (exact, case-sensitive, no whitespace processing); binary search. */
template <XmlEnum E>
constexpr std::optional<E> enumFromXml(std::string_view lexical) noexcept {
	const auto& sorted = EnumTraits<E>::xmlSorted;
	size_t low = 0;
	size_t high = sorted.size();
	while (low < high) {
		size_t mid = low + (high - low) / 2;
		std::string_view name = sorted[mid].name;
		if (name < lexical)
			low = mid + 1;
		else if (lexical < name)
			high = mid;
		else
			return sorted[mid].value;
	}
	return std::nullopt;
}

/** Java name() of a constant; empty for a value outside the table. */
template <XmlEnum E>
constexpr std::string_view enumName(E value) noexcept {
	const auto& names = EnumTraits<E>::names;
	auto ordinal = static_cast<size_t>(enumOrdinal(value));
	return ordinal < names.size() ? names[ordinal] : std::string_view{};
}

/** Java E.valueOf(name) without the exception: looks up by constant name (not by @XmlEnumValue); linear, for configs and rare lookups. */
template <XmlEnum E>
constexpr std::optional<E> enumFromName(std::string_view name) noexcept {
	const auto& names = EnumTraits<E>::names;
	for (size_t ordinal = 0; ordinal < names.size(); ++ordinal) {
		if (names[ordinal] == name)
			return static_cast<E>(ordinal);
	}
	return std::nullopt;
}

} // namespace aion::gameserver::xml
