#pragma once

#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>

/**
 * Java: com.aionemu.commons.utils.GenericValidator - null-or-empty checks. C++ values cannot be null, so the value overloads only test for
 * emptiness (or zero); the pointer and std::optional overloads cover Java's null references.
 */
namespace aion::commons::utils::GenericValidator {

namespace detail {

template <typename T>
concept HasEmpty = requires(const T& t) {
	{ t.empty() } -> std::convertible_to<bool>;
};

template <typename T>
concept Number = std::is_arithmetic_v<T> && !std::same_as<T, bool>;

} // namespace detail

/** Java: isBlankOrNull(String). Note: like Java, a string of spaces is not blank. */
constexpr bool isBlankOrNull(std::string_view s) noexcept {
	return s.empty();
}

constexpr bool isBlankOrNull(const char* s) noexcept {
	return s == nullptr || *s == '\0';
}

/** Java: isBlankOrNull(Collection) / isBlankOrNull(Map) / isBlankOrNull(Object[]) - any container with empty(). */
template <detail::HasEmpty C>
constexpr bool isBlankOrNull(const C& container) noexcept(noexcept(container.empty())) {
	return container.empty();
}

/** Nullable collection (Java: a null reference). */
template <detail::HasEmpty C>
constexpr bool isBlankOrNull(const C* container) noexcept(noexcept(container->empty())) {
	return container == nullptr || container->empty();
}

/** Java: isBlankOrNull(Number) - true if zero. */
template <detail::Number N>
constexpr bool isBlankOrNull(N n) noexcept {
	return static_cast<double>(n) == 0;
}

/** Java: isBlankOrNull(Number) for a nullable number (Integer, Long, ...) - true if empty or zero. */
template <detail::Number N>
constexpr bool isBlankOrNull(const std::optional<N>& n) noexcept {
	return !n || static_cast<double>(*n) == 0;
}

} // namespace aion::commons::utils::GenericValidator
