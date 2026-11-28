#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "aion/commons/utils/JavaFile.h"

using namespace aion::commons::utils;

namespace {

std::vector<std::filesystem::path> sorted(std::vector<std::filesystem::path> files) {
	JavaFile::sort(files);
	return files;
}

} // namespace

// upstream 2cd926056: Files.find(...).map(Path::toFile).sorted()
TEST(JavaFileTest, SortsLikeFileCompareTo) {
	std::vector<std::filesystem::path> files{"d/sub/2.xml", "d/Zeta.xml", "d/sub/10.xml", "d/item_b.xml", "d/sub2.xml", "d/_under.xml",
	                                         "d/item.xml", "d/alpha.xml", "d/sub-x.xml"};
#ifdef _WIN32
	// WinNTFileSystem: compareToIgnoreCase of "d\...": '-' (0x2D) < '2' (0x32) < '\' (0x5C) < '_' (0x5F) < letters
	EXPECT_EQ(sorted(files), (std::vector<std::filesystem::path>{"d/_under.xml", "d/alpha.xml", "d/item.xml", "d/item_b.xml", "d/sub-x.xml",
	                                                             "d/sub2.xml", "d/sub/10.xml", "d/sub/2.xml", "d/Zeta.xml"}));
	EXPECT_EQ(JavaFile::compare("D/A.XML", "d\\a.xml"), 0);
	EXPECT_LT(JavaFile::compare("a.xml", "B.xml"), 0);
#else
	// UnixFileSystem: compareTo of "d/...": '-' < '/' < '2' < 'Z' < '_' < lower-case letters
	EXPECT_EQ(sorted(files), (std::vector<std::filesystem::path>{"d/Zeta.xml", "d/_under.xml", "d/alpha.xml", "d/item.xml", "d/item_b.xml",
	                                                             "d/sub-x.xml", "d/sub/10.xml", "d/sub/2.xml", "d/sub2.xml"}));
	EXPECT_GT(JavaFile::compare("a.xml", "B.xml"), 0);
#endif
	EXPECT_EQ(JavaFile::compare("a.xml", "a.xml"), 0);
}
