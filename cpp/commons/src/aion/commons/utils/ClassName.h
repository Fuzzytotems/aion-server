#pragma once

#include <string>
#include <string_view>
#include <typeinfo>

/**
 * Readable names of C++ types, the counterpart of Java's Class.getName() / getSimpleName() / isAnonymousClass(). Names use C++ spelling
 * ("aion::gameserver::network::aion::serverpackets::SM_MESSAGE"), independent of the compiler: MSVC's "class "/"struct " keywords are removed
 * and GCC/Clang names are demangled.
 */
namespace aion::commons::utils {

/** Java: Class.getName() - the fully qualified type name, e.g. "aion::commons::utils::IllegalArgumentException" */
std::string getClassName(const std::type_info& type);

/**
 * Java: Class.getSimpleName() - the unqualified type name (template arguments are kept), e.g. "IllegalArgumentException". For lambdas this is the
 * compiler's name of the closure type, e.g. "&lt;lambda_1&gt;".
 */
std::string getSimpleClassName(const std::type_info& type);

/** Java: Class.isAnonymousClass() - true for closure types of lambda expressions. */
bool isAnonymousClass(const std::type_info& type);

namespace detail {

/** Normalizes a (demangled) compiler type name: removes MSVC's class/struct/union/enum keywords. Exposed for tests. */
std::string normalizeTypeName(std::string_view name);

/** @return the part of a normalized type name after the last top-level "::". Exposed for tests. */
std::string_view simpleTypeName(std::string_view normalizedName);

} // namespace detail

} // namespace aion::commons::utils
