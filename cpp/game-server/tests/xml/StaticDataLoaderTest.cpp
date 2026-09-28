// The loader skeleton (docs/design/static-data.md §3.2): per-import parallel parsing, sequential binding in import order, singleRootTag
// merging of directory imports, holder registry and filter, holder dependencies, IDREFs across holders, stats over several files.

#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "XmlTestModel.h"
#include "XmlTestSupport.h"

namespace aion::gameserver::xml::test {
namespace {

class StaticDataLoaderTest : public testing::Test {
protected:
	void SetUp() override {
		hookCalls.clear();
		registry.add(HolderRegistration::of<ItemData>("item_templates"));
		registry.add(HolderRegistration::of<PlayerInitialData>("player_initial_data", {typeid(ItemData)}));
		registry.add(HolderRegistration::of<UndeclaredReader>("undeclared_reader"));
	}

	std::filesystem::path writeStaticData(std::string_view imports) const {
		return dir.write("static_data.xml", "<static_data>\n" + std::string(imports) + "\n</static_data>");
	}

	static std::string itemsFile(int32_t firstId, int32_t count, std::string_view rootAttributes = "") {
		std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<item_templates " + std::string(rootAttributes) + ">\n";
		for (int32_t i = 0; i < count; ++i)
			xml += "\t<item_template id=\"" + std::to_string(firstId + i) + "\" name=\"item" + std::to_string(firstId + i) + "\"/>\n";
		return xml + "</item_templates>\n";
	}

	TempDir dir;
	HolderRegistry registry;
};

TEST_F(StaticDataLoaderTest, DirectoryImportMergesAllFilesIntoOneHolder) {
	dir.write("items/b.xml", itemsFile(3, 2, R"(version="ignored" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance")"));
	dir.write("items/a/1.xml", itemsFile(1, 2, R"(version="first")"));
	dir.write("items/c.xml", itemsFile(5, 1));
	std::filesystem::path main = writeStaticData(R"(<import file="items" singleRootTag="true"/>)");

	for (bool parallel : {false, true}) {
		hookCalls.clear();
		LoadOptions options;
		options.collectStats = true;
		options.parallelParse = parallel;
		LoadContext context(options);
		StaticDataRoot root;
		context.setRoot(XmlParent::of(root));
		StaticDataLoader(registry).load(context, main);

		EXPECT_EQ(context.holderTags(), (std::vector<std::string>{"item_templates"}));
		std::unique_ptr<ItemData> items = context.takeHolder<ItemData>();
		ASSERT_NE(items, nullptr);
		EXPECT_FALSE(context.hasHolder("item_templates"));
		EXPECT_EQ(items->getVersion(), "first") << "root attributes come from the first file only";
		std::vector<int32_t> ids;
		for (const ItemTemplate& item : items->getItems())
			ids.push_back(item.getTemplateId());
		EXPECT_EQ(ids, (std::vector<int32_t>{1, 2, 3, 4, 5})) << "files in depth-first order, children in document order";
		EXPECT_EQ(items->getItems().capacity(), 5u) << "reserved over all files of the import";
		EXPECT_TRUE(items->hookSawStaticDataRoot());
		EXPECT_EQ(std::ranges::count(hookCalls, std::string("ItemData:5")), 1) << "the holder hook runs once, after all files";
		EXPECT_EQ(hookCalls.back(), "ItemData:5");

		const BindStats& stats = context.stats();
		EXPECT_EQ(stats.element("item_templates"), (NodeCounts{1, 0, 0})) << "only the first root is part of the merged document";
		EXPECT_EQ(stats.element("item_template"), (NodeCounts{5, 0, 0}));
		EXPECT_EQ(stats.attribute("item_templates", "version"), (NodeCounts{1, 0, 0})) << "later root attributes are not counted per tag";
		EXPECT_EQ(stats.attribute("item_templates", "xmlns:xsi"), NodeCounts{});
		ASSERT_EQ(stats.skippedRoots().size(), 2u) << "the roots of later files, dropped by the merge";
		EXPECT_EQ(dir.relative(stats.skippedRoots()[0].file), "items/b.xml");
		EXPECT_EQ(stats.skippedRoots()[0].tag, "item_templates");
		EXPECT_EQ(stats.skippedRoots()[0].attributes, (std::vector<std::string>{"version"})) << "namespace declarations are not listed";
		EXPECT_EQ(dir.relative(stats.skippedRoots()[1].file), "items/c.xml");
		EXPECT_TRUE(stats.skippedRoots()[1].attributes.empty());
		EXPECT_EQ(stats.namespaceDeclarations(), 0u) << "the declaration was on a skipped root";
		EXPECT_EQ(context.xmlIdCount(), 5u);
		std::ostringstream totals;
		stats.writeTotals(totals, dir.path().generic_string());
		EXPECT_NE(totals.str().find("\"file\": \"items/b.xml\""), std::string::npos) << totals.str();
	}
}

TEST_F(StaticDataLoaderTest, ParallelParsingOfManyFilesKeepsFileOrder) {
	for (int32_t file = 0; file < 24; ++file) {
		char name[32];
		std::snprintf(name, sizeof(name), "items/f%02d.xml", file);
		dir.write(name, itemsFile(file * 100, 50));
	}
	std::filesystem::path main = writeStaticData(R"(<import file="items" singleRootTag="true"/>)");
	std::vector<int32_t> expected;
	for (int32_t file = 0; file < 24; ++file)
		for (int32_t i = 0; i < 50; ++i)
			expected.push_back(file * 100 + i);

	for (bool parallel : {true, false}) {
		LoadOptions options;
		options.parallelParse = parallel;
		LoadContext context(options);
		StaticDataLoader(registry).load(context, main);
		std::vector<int32_t> ids;
		for (const ItemTemplate& item : context.findHolder<ItemData>()->getItems())
			ids.push_back(item.getTemplateId());
		EXPECT_EQ(ids, expected) << "parallel=" << parallel;
	}
}

TEST_F(StaticDataLoaderTest, ParseErrorsOfAnyFileAbortTheLoad) {
	for (int32_t file = 0; file < 8; ++file)
		dir.write("items/f" + std::to_string(file) + ".xml", file == 5 ? "<item_templates>\n<item_template id=\"1\" name=\"x\">\n" : itemsFile(file, 3));
	std::filesystem::path main = writeStaticData(R"(<import file="items" singleRootTag="true"/>)");
	LoadContext context;
	EXPECT_STATIC_DATA_ERROR(StaticDataLoader(registry).load(context, main), "items/f5.xml:", "XML parse error");
}

TEST_F(StaticDataLoaderTest, DifferentRootTagsInOneDirectoryImport) {
	dir.write("mixed/a.xml", itemsFile(1, 1));
	dir.write("mixed/b.xml", "<item_template_list>\n\t<item_template id=\"2\" name=\"b\"/>\n</item_template_list>");
	std::filesystem::path main = writeStaticData(R"(<import file="mixed" singleRootTag="true"/>)");
	{
		LoadContext context;
		EXPECT_STATIC_DATA_ERROR(StaticDataLoader(registry).load(context, main), "mixed/b.xml:1:2: item_template_list (ItemData)",
		                         "Root <item_template_list> differs from <item_templates>");
	}
	LoadOptions options;
	options.strict = false;
	LoadContext context(options);
	StaticDataLoader(registry).load(context, main);
	EXPECT_EQ(context.findHolder<ItemData>()->getItems().size(), 2u) << "XmlMerger merges the children anyway";
}

TEST_F(StaticDataLoaderTest, HoldersBindInImportOrderWithDependenciesAndIdRefs) {
	dir.write("items.xml", itemsFile(100, 3));
	dir.write("player_initial_data.xml", R"(<player_initial_data>
	<player_data race="ASMODIANS" weapon="101"><item>102</item></player_data>
	<elyos_spawn_location map_id="1"/>
</player_initial_data>)");
	std::filesystem::path main = writeStaticData(R"(<import file="items.xml"/>
<import file="player_initial_data.xml"/>)");
	LoadContext context;
	StaticDataLoader(registry).load(context, main);
	EXPECT_EQ(context.holderTags(), (std::vector<std::string>{"item_templates", "player_initial_data"}));
	context.resolveIdRefs();
	const ItemData* items = context.findHolder<ItemData>();
	const PlayerInitialData* players = context.findHolder<PlayerInitialData>();
	EXPECT_EQ(players->itemsSeenByHook(), 3u);
	EXPECT_EQ(players->getPlayers().at(0).weapon, items->getItemTemplate(101));
	EXPECT_EQ(players->getPlayers().at(0).items.at(0), items->getItemTemplate(102));
	EXPECT_TRUE(players->idRefTaskRan());
	EXPECT_EQ(hookCalls.back(), "PlayerInitialData");
}

TEST_F(StaticDataLoaderTest, HookReadsOfUndeclaredDependenciesThrow) {
	dir.write("items.xml", itemsFile(1, 1));
	dir.write("reader.xml", "<undeclared_reader/>");
	std::filesystem::path main = writeStaticData(R"(<import file="items.xml"/><import file="reader.xml"/>)");
	LoadContext context;
	try {
		StaticDataLoader(registry).load(context, main);
		FAIL() << "expected IllegalStateException";
	} catch (const commons::utils::IllegalStateException& e) {
		EXPECT_NE(std::string(e.what()).find("Holder <undeclared_reader> reads"), std::string::npos) << e.what();
		EXPECT_NE(std::string(e.what()).find("not a declared dependency"), std::string::npos) << e.what();
	}
}

TEST_F(StaticDataLoaderTest, HolderFilterSkipsOtherImportsWithoutParsingThem) {
	dir.write("items.xml", itemsFile(1, 2));
	dir.write("broken.xml", "<!-- first comment --><?pi x?>\n<zone_data><not closed");
	dir.write("player_initial_data.xml", "<player_initial_data><elyos_spawn_location/></player_initial_data>");
	std::filesystem::path main = writeStaticData(R"(<import file="items.xml"/><import file="broken.xml"/><import file="player_initial_data.xml"/>)");

	ItemData published;
	LoadOptions options;
	options.holders = std::set<std::string, std::less<>>{"player_initial_data"};
	LoadContext context(options);
	context.setPublishedHolderLookup([&](std::type_index type) -> const void* { return type == typeid(ItemData) ? &published : nullptr; });
	StaticDataLoader(registry).load(context, main);
	EXPECT_EQ(context.holderTags(), (std::vector<std::string>{"player_initial_data"}));
	EXPECT_EQ(context.findHolder<ItemData>(), &published) << "a holder outside this load comes from the published lookup";

	LoadContext unfiltered;
	EXPECT_STATIC_DATA_ERROR(StaticDataLoader(registry).load(unfiltered, main), "broken.xml:2:", "XML parse error");
}

TEST_F(StaticDataLoaderTest, UnknownAndDuplicateHolderTags) {
	dir.write("items.xml", itemsFile(1, 1));
	dir.write("items2.xml", itemsFile(2, 1));
	dir.write("zones.xml", "<?xml version=\"1.0\"?>\n<zone_data/>");
	{
		LoadContext context;
		EXPECT_STATIC_DATA_ERROR(StaticDataLoader(registry).load(context, writeStaticData(R"(<import file="zones.xml"/>)")),
		                         "zones.xml:2:2: Unknown static data holder <zone_data>");
	}
	std::filesystem::path twice = writeStaticData(R"(<import file="items.xml"/><import file="items2.xml"/>)");
	{
		LoadContext context;
		EXPECT_STATIC_DATA_ERROR(StaticDataLoader(registry).load(context, twice), "Holder <item_templates> is imported more than once");
	}
	LoadOptions options;
	options.strict = false;
	LoadContext context(options);
	StaticDataLoader(registry).load(context, twice);
	ASSERT_EQ(context.holderTags().size(), 1u);
	EXPECT_EQ(context.findHolder<ItemData>()->getItems().at(0).getTemplateId(), 2) << "the later import replaces the earlier one";
	EXPECT_NE(context.findXmlId<ItemTemplate>("1"), nullptr) << "the replaced holder stays alive for its XmlIDs";
	EXPECT_EQ(context.retiredCount(), 1u);
}

TEST_F(StaticDataLoaderTest, ADuplicateHolderInStrictModeRollsBackItsIdRefsAndTasks) {
	dir.write("items.xml", itemsFile(100, 3));
	constexpr const char* players = R"(<player_initial_data>
	<player_data race="ASMODIANS" weapon="101"><item>102</item></player_data>
	<elyos_spawn_location map_id="1"/>
</player_initial_data>)";
	dir.write("players1.xml", players);
	dir.write("players2.xml", players);
	std::filesystem::path main = writeStaticData(R"(<import file="items.xml"/><import file="players1.xml"/><import file="players2.xml"/>)");
	LoadContext context;
	EXPECT_STATIC_DATA_ERROR(StaticDataLoader(registry).load(context, main), "Holder <player_initial_data> is imported more than once");
	EXPECT_EQ(context.holderTags(), (std::vector<std::string>{"item_templates", "player_initial_data"}));
	EXPECT_EQ(context.pendingIdRefs(), 2u) << "only the IDREF slots of the first, kept holder; the destroyed duplicate's are rolled back";
	EXPECT_EQ(context.xmlIdCount(), 3u);
	EXPECT_NO_THROW(context.resolveIdRefs()) << "the context stays usable after the error";
	const PlayerInitialData* kept = context.findHolder<PlayerInitialData>();
	ASSERT_NE(kept, nullptr);
	EXPECT_EQ(kept->getPlayers().at(0).weapon, context.findHolder<ItemData>()->getItemTemplate(101));
	EXPECT_TRUE(kept->idRefTaskRan());
}

TEST_F(StaticDataLoaderTest, RegistryAndErasedHolders) {
	EXPECT_THROW(registry.add(HolderRegistration::of<ItemData>("item_templates")), commons::utils::IllegalArgumentException);
	const HolderRegistration* registration = registry.find("player_initial_data");
	ASSERT_NE(registration, nullptr);
	EXPECT_EQ(registration->className, "PlayerInitialData");
	EXPECT_EQ(registration->type, std::type_index(typeid(PlayerInitialData)));
	EXPECT_EQ(registration->dependencies, (std::vector<std::type_index>{typeid(ItemData)}));
	EXPECT_EQ(registry.find("npc_templates"), nullptr);

	ErasedHolder holder = registration->create();
	EXPECT_EQ(holder.type(), std::type_index(typeid(PlayerInitialData)));
	EXPECT_THROW(holder.take<ItemData>(), commons::utils::IllegalStateException);
	std::unique_ptr<PlayerInitialData> taken = holder.take<PlayerInitialData>();
	EXPECT_NE(taken, nullptr);
	EXPECT_FALSE(holder);

	LoadContext context;
	EXPECT_EQ(context.takeHolder<ItemData>(), nullptr);
	EXPECT_EQ(context.findHolder<ItemData>(), nullptr);
	EXPECT_THROW(context.holder<ItemData>(), commons::utils::IllegalStateException);
}

TEST_F(StaticDataLoaderTest, PeekRootTagAndBindFile) {
	std::filesystem::path file =
	  dir.write("a.xml", "\xEF\xBB\xBF<?xml version=\"1.0\"?>\n<!-- <nope> -->\n<!DOCTYPE x>\n<item_templates\tversion=\"2\"/>");
	EXPECT_EQ(StaticDataLoader::peekRootTag(file), "item_templates");
	EXPECT_EQ(StaticDataLoader::peekRootTag(dir.write("b.xml", "<a/>")), "a");
	EXPECT_EQ(StaticDataLoader::peekRootTag(dir.write("c.xml", "no xml")), "");
	LoadContext context;
	std::unique_ptr<ItemData> items = bindFile<ItemData>(context, file);
	EXPECT_EQ(items->getVersion(), "2");
	EXPECT_STATIC_DATA_ERROR(bindFile<ItemData>(context, dir.path() / "missing.xml"), "Cannot open", "missing.xml");
}

TEST_F(StaticDataLoaderTest, LoadFilesBindsAHolderWithoutStaticDataXml) {
	std::vector<std::filesystem::path> files{dir.write("x/1.xml", itemsFile(1, 1)), dir.write("x/2.xml", itemsFile(2, 1))};
	LoadContext context;
	StaticDataLoader(registry).loadFiles(context, files);
	EXPECT_EQ(context.findHolder<ItemData>()->size(), 2u);
}

} // namespace
} // namespace aion::gameserver::xml::test

namespace aion::gameserver::xml::test {
namespace {

TEST(StaticDataRealFilesTest, EveryImportedFileParsesAndMatchesAnIndependentNodeCount) {
	std::filesystem::path main = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data/static_data.xml";
	if (!std::filesystem::exists(main))
		GTEST_SKIP() << "Java data tree not found: " << main;
	auto start = std::chrono::steady_clock::now();
	size_t files = 0;
	uint64_t elements = 0;
	uint64_t attributes = 0;
	uint64_t bytes = 0;
	for (const StaticDataImport& entry : StaticDataImports::resolve(main, 0, true)) {
		std::vector<std::unique_ptr<XmlDocument>> documents = StaticDataLoader::parseFiles(entry.files, true);
		for (const auto& document : documents) {
			++files;
			bytes += document->size();
			std::vector<pugi::xml_node> pending{document->root()};
			while (!pending.empty()) {
				pugi::xml_node node = pending.back();
				pending.pop_back();
				++elements;
				for (pugi::xml_attribute attribute = node.first_attribute(); attribute; attribute = attribute.next_attribute())
					++attributes;
				for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
					if (child.type() == pugi::node_element)
						pending.push_back(child);
				}
			}
		}
	}
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	std::cout << "parsed " << files << " files, " << bytes << " bytes, " << elements << " elements, " << attributes << " attributes in " << millis
	          << " ms\n";
	// expected values: Python xml.parsers.expat over the same 664 files (xmlns declarations counted as attributes)
	EXPECT_EQ(files, 664u);
	EXPECT_EQ(elements, 2106068u);
	EXPECT_EQ(attributes, 6661924u);
}

} // namespace
} // namespace aion::gameserver::xml::test
