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

TEST(StaticDataImportsTest, DirectoriesAreListedDepthFirstInUppercaseOrdinalOrder) {
	TempDir dir;
	for (std::string_view file : {"d/Zeta.xml", "d/alpha.xml", "d/_under.xml", "d/item_b.xml", "d/item.xml", "d/UPPER.XML", "d/notes.txt",
	                              "d/a.xml.bak", "d/sub/2.xml", "d/sub/10.xml", "d/sub/deeper/x.xml", "d/Sub2/y.xml"})
		dir.write(file, "<r/>");
	dir.mkdir("d/empty");

	std::vector<std::string> expected{"d/alpha.xml",        "d/item.xml",   "d/item_b.xml", "d/sub/10.xml", "d/sub/2.xml",
	                                  "d/sub/deeper/x.xml", "d/Sub2/y.xml", "d/UPPER.XML",  "d/Zeta.xml",   "d/_under.xml"};
	EXPECT_EQ(relativeAll(dir, StaticDataImports::listFiles(dir.path() / "d", true)), expected)
	  << "'.' < '_' and 'A'..'Z' < '_': ITEM.XML before ITEM_B.XML, ZETA.XML before _UNDER.XML; directories descended in place";
	EXPECT_EQ(relativeAll(dir, StaticDataImports::listFiles(dir.path() / "d", false)),
	          (std::vector<std::string>{"d/alpha.xml", "d/item.xml", "d/item_b.xml", "d/UPPER.XML", "d/Zeta.xml", "d/_under.xml"}));

	EXPECT_LT(StaticDataImports::orderKey("a"), StaticDataImports::orderKey("B"));
	EXPECT_EQ(StaticDataImports::orderKey(u8"été.xml"), u"ÉTÉ.XML");

#ifdef _WIN32
	// the comparator is what NTFS enumeration returns (FindFirstFile / Files.find on Windows)
	for (const char* sub : {"d", "d/sub"}) {
		std::vector<std::u16string> native;
		for (const auto& entry : std::filesystem::directory_iterator(dir.path() / sub))
			native.push_back(entry.path().filename().u16string());
		std::vector<std::u16string> sorted = native;
		std::ranges::sort(sorted, [](const auto& a, const auto& b) { return StaticDataImports::orderKey(a) < StaticDataImports::orderKey(b); });
		EXPECT_EQ(native, sorted) << sub;
	}
#endif
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
	ASSERT_EQ(imports.size(), 92u);
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
	EXPECT_EQ(directories, 12u);
	EXPECT_EQ(directoryFiles, 584u);
	EXPECT_EQ(files, 664u);
	EXPECT_EQ(imports.front().file, "items/item_templates.xml");
	EXPECT_EQ(imports.back().file, "skills/signet_data_templates.xml");

	auto goods = std::ranges::find(imports, std::string("goodslists/goodslists.xml"), &StaticDataImport::file);
	ASSERT_NE(goods, imports.end());
	EXPECT_EQ(goods->files.front().filename(), "goodslists.xml");
	std::vector<StaticDataImport> europe = StaticDataImports::resolve(main, 2, true);
	auto goodsEurope = std::ranges::find(europe, std::string("goodslists/goodslists.xml"), &StaticDataImport::file);
	EXPECT_EQ(goodsEurope->files.front().filename(), "goodslists_europe.xml");

#ifdef _WIN32
	// every imported directory level enumerates in the order of the comparator (the design's NTFS claim, 584 files)
	for (const StaticDataImport& entry : imports) {
		if (!entry.directory)
			continue;
		std::vector<std::filesystem::path> native;
		std::vector<std::filesystem::path> pending{entry.path};
		while (!pending.empty()) {
			std::filesystem::path current = pending.back();
			pending.pop_back();
			std::vector<std::u16string> names;
			for (const auto& child : std::filesystem::directory_iterator(current)) {
				names.push_back(child.path().filename().u16string());
				if (child.is_directory())
					pending.push_back(child.path());
			}
			std::vector<std::u16string> sorted = names;
			std::ranges::stable_sort(sorted, [](const auto& a, const auto& b) { return StaticDataImports::orderKey(a) < StaticDataImports::orderKey(b); });
			EXPECT_EQ(names, sorted) << current.generic_string();
		}
	}
#endif
}

} // namespace
} // namespace aion::gameserver::xml::test
