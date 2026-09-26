#include "aion/commons/network/detail/TypeName.h"

#include <cstdlib>
#include <memory>
#include <string_view>

#if defined(__GNUG__) && !defined(_MSC_VER)
#include <cxxabi.h>
#endif

namespace aion::commons::network::detail {

namespace {

std::string demangle(const char* name) {
#if defined(__GNUG__) && !defined(_MSC_VER)
	int status = 0;
	std::unique_ptr<char, decltype(&std::free)> demangled(abi::__cxa_demangle(name, nullptr, nullptr, &status), &std::free);
	if (status == 0 && demangled)
		return demangled.get();
#endif
	return name;
}

} // namespace

std::string simpleTypeName(const std::type_info& type) {
	std::string name = demangle(type.name());
	std::string_view view = name;

	// remove template arguments (they may contain "::" themselves)
	if (size_t templateStart = view.find('<'); templateStart != std::string_view::npos)
		view = view.substr(0, templateStart);
	// remove namespaces and enclosing classes
	if (size_t lastScope = view.rfind("::"); lastScope != std::string_view::npos)
		view = view.substr(lastScope + 2);
	// MSVC prefixes the elaborated type specifier
	for (std::string_view prefix : {"class ", "struct ", "union ", "enum "}) {
		if (view.starts_with(prefix)) {
			view.remove_prefix(prefix.size());
			break;
		}
	}
	return std::string(view);
}

} // namespace aion::commons::network::detail
