#include "aion/commons/utils/ClassName.h"

#include <array>
#include <cstdlib>
#include <memory>

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#define AION_HAS_CXXABI 1
#endif

namespace aion::commons::utils {

namespace {

bool isIdentifierChar(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

std::string demangle(const char* name) {
#ifdef AION_HAS_CXXABI
	int status = 0;
	std::unique_ptr<char, decltype(&std::free)> demangled(abi::__cxa_demangle(name, nullptr, nullptr, &status), &std::free);
	if (status == 0 && demangled)
		return demangled.get();
#endif
	return name;
}

} // namespace

namespace detail {

std::string normalizeTypeName(std::string_view name) {
	static constexpr std::array<std::string_view, 4> KEYWORDS = {"class ", "struct ", "union ", "enum "};
	std::string result;
	result.reserve(name.size());
	for (size_t i = 0; i < name.size();) {
		bool atWordStart = i == 0 || !isIdentifierChar(name[i - 1]);
		bool skipped = false;
		if (atWordStart) {
			for (std::string_view keyword : KEYWORDS) {
				if (name.substr(i).starts_with(keyword)) {
					i += keyword.size();
					skipped = true;
					break;
				}
			}
		}
		if (!skipped)
			result += name[i++];
	}
	return result;
}

std::string_view simpleTypeName(std::string_view normalizedName) {
	int depth = 0; // nesting in <>, () and MSVC's `' quotes
	size_t start = 0;
	for (size_t i = 0; i < normalizedName.size(); i++) {
		char c = normalizedName[i];
		if (c == '<' || c == '(' || c == '`' || c == '{')
			depth++;
		else if ((c == '>' || c == ')' || c == '\'' || c == '}') && depth > 0)
			depth--;
		else if (c == ':' && depth == 0 && i + 1 < normalizedName.size() && normalizedName[i + 1] == ':')
			start = ++i + 1;
	}
	return normalizedName.substr(start);
}

} // namespace detail

std::string getClassName(const std::type_info& type) {
	return detail::normalizeTypeName(demangle(type.name()));
}

std::string getSimpleClassName(const std::type_info& type) {
	return std::string(detail::simpleTypeName(getClassName(type)));
}

bool isAnonymousClass(const std::type_info& type) {
	std::string simpleName = getSimpleClassName(type);
	// MSVC: <lambda_1>, GCC: {lambda()#1}, Clang: (lambda at file:line:column)
	return simpleName.starts_with("<lambda") || simpleName.starts_with("{lambda") || simpleName.starts_with("(lambda");
}

} // namespace aion::commons::utils
