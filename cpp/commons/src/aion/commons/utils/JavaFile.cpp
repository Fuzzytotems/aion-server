#include "aion/commons/utils/JavaFile.h"

#include <algorithm>
#include <string>

#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::utils::JavaFile {

int32_t compare(const std::filesystem::path& a, const std::filesystem::path& b) {
#ifdef _WIN32
	// WinNTFileSystem.compare: f1.getPath().compareToIgnoreCase(f2.getPath()); java.io.File keeps '\' separators only
	std::filesystem::path preferredA = a;
	std::filesystem::path preferredB = b;
	preferredA.make_preferred();
	preferredB.make_preferred();
	std::u16string pathA = preferredA.u16string();
	std::u16string pathB = preferredB.u16string();
	return StringUtils::compareToIgnoreCase(pathA, pathB);
#else
	// UnixFileSystem.compare: f1.getPath().compareTo(f2.getPath())
	std::u16string pathA = a.u16string();
	std::u16string pathB = b.u16string();
	int32_t comparison = pathA.compare(pathB);
	return comparison < 0 ? -1 : (comparison > 0 ? 1 : 0);
#endif
}

void sort(std::vector<std::filesystem::path>& files) {
	std::ranges::stable_sort(files, [](const std::filesystem::path& a, const std::filesystem::path& b) { return compare(a, b) < 0; });
}

} // namespace aion::commons::utils::JavaFile
