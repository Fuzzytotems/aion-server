// Import resolution with XmlMerger's rules (docs/design/static-data.md §3.1): region override, directory depth-first NTFS order,
// singleRootTag/recursiveImport, errors; plus a check over the real data/static_data tree.

#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "XmlTestSupport.h"

namespace aion::gameserver::xml::test {
namespace {

std::vector<std::string> relativeAll(const TempDir& dir, const std::vector<std::filesystem::path>& files) {
	std::vector<std::string> result;
	for (const auto& file : files)
		result.push_back(dir.relative(file));
	return result;
}

TEST(StaticDataImportsTest, CountryRegions) {
	EXPECT_EQ(StaticDataImports::countryRegion(1), "usa");
	EXPECT_EQ(StaticDataImports::countryRegion(2), "europe");
	EXPECT_EQ(StaticDataImports::countryRegion(3), "");
	EXPECT_EQ(StaticDataImports::countryRegion(4), "japan");
	EXPECT_EQ(StaticDataImports::countryRegion(5), "china");
	EXPECT_EQ(StaticDataImports::countryRegion(6), "taiwan");
	EXPECT_EQ(StaticDataImports::countryRegion(7), "russia");
	EXPECT_EQ(StaticDataImports::countryRegion(0), "");
}

TEST(StaticDataImportsTest, RegionOverrideAppliesOnlyToRegularFiles) {
	TempDir dir;
	dir.write("goods/list.xml", "<a/>");
	dir.write("goods/list_europe.xml", "<a/>");
	dir.mkdir("goods/list_usa.xml");
	dir.write("goods/archive.tar.xml", "<a/>");
	dir.write("goods/archive.tar_japan.xml", "<a/>");
	dir.mkdir("npcs");
	dir.mkdir("npcs_europe");
	dir.write("spawns", "");
	dir.write("spawns_russia", "");

	auto relativeOverride = [&](std::string_view file, int32_t country) {
		return dir.relative(StaticDataImports::applyCountryOverride(dir.path() / file, country));
	};
	EXPECT_EQ(relativeOverride("goods/list.xml", 2), "goods/list_europe.xml");
	EXPECT_EQ(relativeOverride("goods/list.xml", 1), "goods/list.xml") << "list_usa.xml is a directory";
	EXPECT_EQ(relativeOverride("goods/list.xml", 3), "goods/list.xml") << "no region for code 3";
	EXPECT_EQ(relativeOverride("goods/archive.tar.xml", 4), "goods/archive.tar_japan.xml") << "split at the last dot";
	EXPECT_EQ(relativeOverride("npcs", 2), "npcs") << "directories are never overridden by directories";
	EXPECT_EQ(relativeOverride("spawns", 7), "spawns_russia") << "a name without dot gets the suffix appended (Java quirk)";
}

// upstream 2cd926056: XmlUtil.listFiles sorts the Files.find results in File.compareTo order
TEST(StaticDataImportsTest, DirectoriesAreListedInFileCompareToOrder) {
	TempDir dir;
	for (std::string_view file : {"d/Zeta.xml", "d/alpha.xml", "d/_under.xml", "d/item_b.xml", "d/item.xml", "d/UPPER.XML", "d/notes.txt",
	                              "d/a.xml.bak", "d/sub/2.xml", "d/sub/10.xml", "d/sub/deeper/x.xml", "d/Sub2/y.xml", "d/sub-x.xml"})
		dir.write(file, "<r/>");
	dir.mkdir("d/empty");

#ifdef _WIN32
	// WinNTFileSystem: compareToIgnoreCase of the paths, '-' (0x2D) < '2' (0x32) < backslash (0x5C) < '_' (0x5F) < letters
	std::vector<std::string> expected{"d/_under.xml", "d/alpha.xml", "d/item.xml",  "d/item_b.xml",       "d/sub-x.xml", "d/Sub2/y.xml",
	                                  "d/sub/10.xml", "d/sub/2.xml", "d/sub/deeper/x.xml", "d/UPPER.XML", "d/Zeta.xml"};
	std::vector<std::string> flat{"d/_under.xml", "d/alpha.xml", "d/item.xml", "d/item_b.xml", "d/sub-x.xml", "d/UPPER.XML", "d/Zeta.xml"};
#else
	// UnixFileSystem: compareTo of the paths, '-' < '/' < '2' < upper case < '_' < lower case
	std::vector<std::string> expected{"d/Sub2/y.xml", "d/UPPER.XML",  "d/Zeta.xml",  "d/_under.xml",       "d/alpha.xml", "d/item.xml",
	                                  "d/item_b.xml", "d/sub-x.xml", "d/sub/10.xml", "d/sub/2.xml", "d/sub/deeper/x.xml"};
	std::vector<std::string> flat{"d/UPPER.XML", "d/Zeta.xml", "d/_under.xml", "d/alpha.xml", "d/item.xml", "d/item_b.xml", "d/sub-x.xml"};
#endif
	EXPECT_EQ(relativeAll(dir, StaticDataImports::listFiles(dir.path() / "d", true)), expected);
	EXPECT_EQ(relativeAll(dir, StaticDataImports::listFiles(dir.path() / "d", false)), flat);
}

TEST(StaticDataImportsTest, ResolvesImportsInDocumentOrder) {
	TempDir dir;
	dir.write("items/items.xml", "<item_templates/>");
	dir.write("goods/goods.xml", "<goods/>");
	dir.write("goods/goods_usa.xml", "<goods/>");
	dir.write("npcs/b.xml", "<npcs/>");
	dir.write("npcs/a/c.xml", "<npcs/>");
	dir.write("flat/x.xml", "<flat/>");
	dir.write("flat/sub/y.xml", "<flat/>");
	std::filesystem::path main = dir.write("static_data.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<static_data xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="static_data.xsd">
	<!-- comment with <import file="nothing"/> -->
	<import file="items/items.xml" />
	<import file="goods/goods.xml" singleRootTag="false"/>
	<import file="npcs/" singleRootTag="TRUE" />
	<import file="flat" singleRootTag="true" recursiveImport="no"/>
</static_data>)");

	std::vector<StaticDataImport> imports = StaticDataImports::resolve(main, 1, true);
	ASSERT_EQ(imports.size(), 4u);
	EXPECT_EQ(imports[0].file, "items/items.xml");
	EXPECT_FALSE(imports[0].directory);
	EXPECT_EQ(relativeAll(dir, imports[0].files), (std::vector<std::string>{"items/items.xml"}));
	EXPECT_EQ(imports[0].line, 4u);
	EXPECT_EQ(relativeAll(dir, imports[1].files), (std::vector<std::string>{"goods/goods_usa.xml"})) << "region override";
	EXPECT_FALSE(imports[1].singleRootTag) << "only read for directories";
	EXPECT_TRUE(imports[2].directory);
	EXPECT_TRUE(imports[2].singleRootTag) << "Boolean.parseBoolean ignores case";
	EXPECT_TRUE(imports[2].recursive);
	EXPECT_EQ(relativeAll(dir, imports[2].files), (std::vector<std::string>{"npcs/a/c.xml", "npcs/b.xml"}));
	EXPECT_FALSE(imports[3].recursive) << "anything but \"true\" is false";
	EXPECT_EQ(relativeAll(dir, imports[3].files), (std::vector<std::string>{"flat/x.xml"}));
	EXPECT_EQ(dir.relative(imports[3].path), "flat");
}

TEST(StaticDataImportsTest, RejectsWhatXmlMergerCannotMerge) {
	TempDir dir;
	dir.write("npcs/a.xml", "<npcs/>");
	dir.mkdir("empty");
	auto resolveText = [&](std::string_view imports, bool strict = true) {
		std::filesystem::path main = dir.write("static_data.xml", "<static_data>\n" + std::string(imports) + "\n</static_data>");
		return StaticDataImports::resolve(main, 0, strict);
	};
	EXPECT_STATIC_DATA_ERROR(resolveText(R"(<import file="missing.xml"/>)"), "static_data.xml:2:2: Missing file to import: ", "missing.xml");
	EXPECT_STATIC_DATA_ERROR(resolveText(R"(<import file="npcs"/>)"), "static_data.xml:2:2: Directory import ", "requires singleRootTag=\"true\"");
	EXPECT_STATIC_DATA_ERROR(resolveText(R"(<import file="empty" singleRootTag="true"/>)"), "contains no .xml files");
	EXPECT_STATIC_DATA_ERROR(resolveText(R"(<import/>)"), "Attribute 'file' is missing");
	EXPECT_STATIC_DATA_ERROR(resolveText(R"(<npcs/>)"), "Unsupported element <npcs>");
	EXPECT_STATIC_DATA_ERROR(resolveText(R"(text)"), "Unexpected text");
	EXPECT_STATIC_DATA_ERROR(resolveText(R"(<import file="npcs/a.xml" skipRoot="true"/>)"), "static_data.xml:2:27: Unknown attribute 'skipRoot'");
	EXPECT_EQ(resolveText(R"(<import file="npcs/a.xml" skipRoot="true"/>)", false).size(), 1u) << "lenient: warning only";
	std::filesystem::path wrongRoot = dir.write("other.xml", "<data><import file=\"npcs/a.xml\"/></data>");
	EXPECT_STATIC_DATA_ERROR(StaticDataImports::resolve(wrongRoot, 0, true), "Root element must be <static_data>");
}

TEST(StaticDataImportsTest, RealStaticDataTreeResolvesLikeXmlMerger) {
	std::filesystem::path main = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data/static_data.xml";
	if (!std::filesystem::exists(main))
		GTEST_SKIP() << "Java data tree not found: " << main;
	std::vector<StaticDataImport> imports = StaticDataImports::resolve(main, 0, true);
	ASSERT_EQ(imports.size(), 94u); // upstream 68eccc7b1: +4 exp_mod_table files, - siege_shields, - curing_objects
	size_t directories = 0;
	size_t directoryFiles = 0;
	size_t files = 0;
	for (const StaticDataImport& entry : imports) {
		files += entry.files.size();
		if (entry.directory) {
			++directories;
			directoryFiles += entry.files.size();
			EXPECT_TRUE(entry.singleRootTag) << entry.file;
		}
	}
	EXPECT_EQ(directories, 13u); // upstream d254ea172 made decomposable_items a directory import
	EXPECT_EQ(directoryFiles, 587u);
	EXPECT_EQ(files, 668u);
	EXPECT_EQ(imports.front().file, "items/item_templates.xml");
	EXPECT_EQ(imports.back().file, "skills/signet_data_templates.xml");

	auto goods = std::ranges::find(imports, std::string("goodslists/goodslists.xml"), &StaticDataImport::file);
	ASSERT_NE(goods, imports.end());
	EXPECT_EQ(goods->files.front().filename(), "goodslists.xml");
	std::vector<StaticDataImport> europe = StaticDataImports::resolve(main, 2, true);
	auto goodsEurope = std::ranges::find(europe, std::string("goodslists/goodslists.xml"), &StaticDataImport::file);
	EXPECT_EQ(goodsEurope->files.front().filename(), "goodslists_europe.xml");

#ifdef _WIN32
	// the real tree has names that the NTFS enumeration order of 51d7a19cb listed differently (upstream 2cd926056)
	auto filesOf = [&](std::string_view import) {
		auto entry = std::ranges::find(imports, std::string(import), &StaticDataImport::file);
		EXPECT_NE(entry, imports.end()) << import;
		std::vector<std::string> relative;
		if (entry != imports.end()) {
			for (const std::filesystem::path& file : entry->files)
				relative.push_back(file.lexically_relative(main.parent_path()).generic_string());
		}
		return relative;
	};
	std::vector<std::string> quests = filesOf("quest_script_data");
	auto position = [](const std::vector<std::string>& list, std::string_view file) { return std::ranges::find(list, file) - list.begin(); };
	EXPECT_LT(position(quests, "quest_script_data/abyss_entry.xml"), position(quests, "quest_script_data/abyssal_splinter.xml"));
	EXPECT_LT(position(quests, "quest_script_data/the_hexway.xml"), position(quests, "quest_script_data/theobomos.xml"));
	EXPECT_EQ(filesOf("zones").front(), "zones/zone_abyss_shields.xml");
#endif
}

} // namespace
} // namespace aion::gameserver::xml::test
