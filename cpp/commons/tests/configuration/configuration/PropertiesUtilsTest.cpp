#include "aion/commons/configuration/PropertiesUtils.h"

#include <atomic>
#include <chrono>
#include <fstream>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"

using namespace aion::commons;
using namespace aion::commons::configuration;
namespace fs = std::filesystem;

namespace {

/** A unique temporary directory, removed after the test. */
class PropertiesUtilsTest : public testing::Test {
protected:
	void SetUp() override {
		static std::atomic<int> counter;
		dir = fs::temp_directory_path() / ("aion_properties_utils_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
		                                   "_" + std::to_string(counter++));
		fs::create_directories(dir);
	}

	void TearDown() override {
		std::error_code ec;
		fs::remove_all(dir, ec);
	}

	void write(const fs::path& relativePath, std::string_view content) {
		fs::path path = dir / relativePath;
		fs::create_directories(path.parent_path());
		std::ofstream out(path, std::ios::binary);
		out << content;
	}

	fs::path dir;
};

} // namespace

TEST_F(PropertiesUtilsTest, LoadMissingFileOrDirectoryYieldsEmptyPropertiesWithDefaults) {
	auto defaults = std::make_shared<Properties>();
	defaults->setProperty("a", "default");
	Properties missing = PropertiesUtils::load(dir / "missing.properties", defaults);
	EXPECT_TRUE(missing.isEmpty());
	EXPECT_EQ(missing.getProperty("a"), "default");
	EXPECT_EQ(missing.getDefaults(), defaults);

	Properties directory = PropertiesUtils::load(dir);
	EXPECT_TRUE(directory.isEmpty());
	EXPECT_EQ(directory.getDefaults(), nullptr);
}

TEST_F(PropertiesUtilsTest, LoadFileAsLatin1) {
	write("my.properties", "a = 1\nb=\xE4\n");
	auto defaults = std::make_shared<Properties>();
	defaults->setProperty("a", "default");
	defaults->setProperty("c", "3");
	Properties p = PropertiesUtils::load(dir / "my.properties", defaults);
	EXPECT_EQ(p.size(), 2u);
	EXPECT_EQ(p.getProperty("a"), "1");
	EXPECT_EQ(p.getProperty("b"), "\xC3\xA4");
	EXPECT_EQ(p.getProperty("c"), "3");
}

TEST_F(PropertiesUtilsTest, MalformedEscapeIsNotWrapped) {
	write("bad.properties", "a=\\u12");
	EXPECT_THROW(PropertiesUtils::load(dir / "bad.properties"), utils::IllegalArgumentException);
}

TEST_F(PropertiesUtilsTest, LoadFromDirectory) {
	write("b.properties", "key=b\nb=1\n");
	write("a.properties", "key=a\na=1\n");
	write("ignored.txt", "txt=1\n");
	write("upper.PROPERTIES", "upper=1\n");
	write("sub/c.properties", "key=c\nc=1\n");
	write("sub/deeper/d.properties", "d=1\n");
	write(".properties", "hidden=1\n");

	Properties flat;
	PropertiesUtils::loadFromDirectory(flat, dir, false);
	EXPECT_EQ(flat.stringPropertyNames(), (std::set<std::string>{"a", "b", "hidden", "key"}));
	EXPECT_EQ(flat.getProperty("key"), "b"); // files are loaded in sorted order, later ones win

	Properties recursive;
	PropertiesUtils::loadFromDirectory(recursive, dir, true);
	EXPECT_EQ(recursive.stringPropertyNames(), (std::set<std::string>{"a", "b", "c", "d", "hidden", "key"}));
	EXPECT_EQ(recursive.getProperty("key"), "c"); // sub/c.properties sorts after b.properties

	Properties existing;
	existing.setProperty("key", "existing");
	existing.setProperty("kept", "1");
	PropertiesUtils::loadFromDirectory(existing, dir / "sub", false);
	EXPECT_EQ(existing.getProperty("key"), "c");
	EXPECT_EQ(existing.getProperty("kept"), "1");
	EXPECT_FALSE(existing.getProperty("d").has_value());
}

TEST_F(PropertiesUtilsTest, LoadFromDirectoryGivenAFile) {
	write("single.properties", "x=1\n");
	Properties p;
	PropertiesUtils::loadFromDirectory(p, dir / "single.properties", false);
	EXPECT_EQ(p.getProperty("x"), "1");
}

TEST_F(PropertiesUtilsTest, LoadFromMissingDirectoryThrows) {
	Properties p;
	EXPECT_THROW(PropertiesUtils::loadFromDirectory(p, dir / "missing", false), utils::IOException);
	EXPECT_THROW(PropertiesUtils::loadFromDirectory(p, dir / "missing", true), utils::IOException);
}
