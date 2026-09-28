#pragma once

#include <string>
#include <typeinfo>

namespace aion::commons::network::detail {

/**
 * Java: Class.getSimpleName(). Returns the unqualified name of a type from its type_info: namespaces, enclosing classes and template arguments
 * are removed, e.g. "aion::loginserver::network::aion::serverpackets::SM_INIT" -> "SM_INIT".
 * <p>
 * The result is derived from the compiler's type name (demangled on GCC/Clang), so it is a best effort for unusual types such as lambdas.
 */
std::string simpleTypeName(const std::type_info& type);

} // namespace aion::commons::network::detail
