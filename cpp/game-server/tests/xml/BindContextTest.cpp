// The binder runtime on fixture XML (docs/design/static-data.md §2.3-§3.2, verification V6): attributes, defaults, nullability, lists,
// @XmlElements, @XmlElementWrapper, @XmlList, IDREF, adapters, required checks, strict vs lenient mode, hook order, error locations, stats.

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "XmlTestModel.h"
#include "XmlTestSupport.h"

namespace aion::gameserver::xml::test {
namespace {

class BindContextTest : public testing::Test {
protected:
	void SetUp() override { hookCalls.clear(); }

	static LoadOptions lenient() {
		LoadOptions options;
		options.strict = false;
		options.collectStats = true;
		return options;
	}
	static LoadOptions strictWithStats() {
		LoadOptions options;
		options.collectStats = true;
		return options;
	}
};

TEST_F(BindContextTest, ScalarAttributesAndDefaults) {
	LoadContext context;
	auto items = bindString<ItemData>(context, R"(<item_templates>
	<item_template id="100" name="Sword" level="-3" weight="300" price="9000000000" speed="2.25" ratio="0.5" tradable="0" race="ELYOS"
		optional_race="ASMODIANS" robot_id="7" alias="" pre_effects="1 2  3" zones="bind recall bind" restrict="1 -2 3" start="2015-02-06T00:00:00"/>
	<item_template id="101" name="Plain"/>
</item_templates>)");
	ASSERT_EQ(items->getItems().size(), 2u);
	const ItemTemplate& full = items->getItems()[0];
	EXPECT_EQ(full.getTemplateId(), 100);
	EXPECT_EQ(full.getName(), "Sword");
	EXPECT_EQ(full.getLevel(), -3);
	EXPECT_EQ(full.getWeight(), 300);
	EXPECT_EQ(full.getPrice(), 9000000000LL);
	EXPECT_EQ(full.getSpeed(), 2.25f);
	EXPECT_EQ(full.getRatio(), 0.5);
	EXPECT_FALSE(full.isTradable());
	EXPECT_EQ(full.getRace(), Race::ELYOS);
	EXPECT_EQ(full.getOptionalRace(), Race::ASMODIANS);
	EXPECT_EQ(full.getRobotId(), 7);
	EXPECT_EQ(full.getAlias(), std::optional<std::string>("")) << "present-empty optional string";
	EXPECT_EQ(full.getPreEffects(), std::optional<std::vector<int32_t>>(std::vector<int32_t>{1, 2, 3}));
	EXPECT_EQ(full.getZones(), (std::unordered_set<ZoneAttribute>{ZoneAttribute::BIND, ZoneAttribute::RECALL}));
	EXPECT_EQ(full.getRestrictions(), (std::vector<int8_t>{1, -2, 3}));
	ASSERT_TRUE(full.getStart().has_value());
	EXPECT_EQ(adapters::printLocalDateTime(*full.getStart()), "2015-02-06T00:00");

	const ItemTemplate& plain = items->getItems()[1];
	EXPECT_EQ(plain.getLevel(), 1) << "Java initializer kept when absent";
	EXPECT_EQ(plain.getSpeed(), 1.5f);
	EXPECT_TRUE(plain.isTradable());
	EXPECT_EQ(plain.getRace(), Race::PC_ALL);
	EXPECT_EQ(plain.getOptionalRace(), std::nullopt) << "enum without initializer is null when absent";
	EXPECT_EQ(plain.getRobotId(), std::nullopt);
	EXPECT_EQ(plain.getAlias(), std::nullopt);
	EXPECT_EQ(plain.getPreEffects(), std::nullopt) << "absent @XmlList differs from present-empty";
	EXPECT_EQ(plain.getRestrictions(), (std::vector<int8_t>{1, 1, 1})) << "mapped adapter initializer";
	EXPECT_FALSE(plain.getStart().has_value());
	EXPECT_EQ(plain.getWeapon(), nullptr);
	EXPECT_TRUE(plain.getStats().empty());
}

TEST_F(BindContextTest, ChildElementsTextListsChoicesAndPostOrderHooks) {
	LoadContext context;
	StaticDataRoot root;
	context.setRoot(XmlParent::of(root));
	auto items = bindString<ItemData>(context, R"(<item_templates version="4.8">
	<item_template id="1" name="A">
		<stat name="power" value="10"/>
		<desc>  multi<![CDATA[ <part> ]]>text </desc>
		<skilllearn skill_id="11" delay="5"/>
		<weapon min_damage="1" max_damage="2" attack_speed="1.5"/>
		<cooldown>
			30
		</cooldown>
		<dye color="red"/>
		<stat name="block" value="-4"/>
		<bonus_stat name="hp"/>
		<use_skill skill_id="12"/>
		<skilllearn skill_id="13"/>
		<comment ignored="yes"><deep/></comment>
	</item_template>
</item_templates>)");
	const ItemTemplate& item = items->getItems().at(0);
	EXPECT_EQ(items->getVersion(), "4.8");
	ASSERT_EQ(item.getStats().size(), 2u) << "interleaved list elements accumulate in document order";
	EXPECT_EQ(item.getStats()[0].name, "power");
	EXPECT_EQ(item.getStats()[1].value, -4);
	EXPECT_EQ(item.getStatsCapacityAtHook(), 2u) << "reserved to the exact child count";
	ASSERT_EQ(item.getBonusStats().size(), 1u);
	EXPECT_EQ(item.getBonusStats()[0]->name, "hp");
	EXPECT_EQ(item.getDescription(), "  multi <part> text ") << "concatenated PCDATA and CDATA, not trimmed";
	EXPECT_EQ(item.getCooldown(), 30) << "numbers in text content are trimmed";
	ASSERT_NE(item.getWeapon(), nullptr);
	EXPECT_EQ(item.getWeapon()->maxDamage, 2);
	EXPECT_EQ(item.getWeapon()->attackSpeed, 1.5f);

	ASSERT_EQ(item.getActions().size(), 3u);
	EXPECT_EQ(item.getActions()[0]->javaClassName(), "SkillAction");
	EXPECT_EQ(item.getActions()[0]->getDelay(), 5) << "base class attribute through the derived binding";
	EXPECT_TRUE(static_cast<const SkillAction&>(*item.getActions()[0]).parentWasItem()) << "hook parent is the concrete enclosing object";
	EXPECT_EQ(item.getActions()[1]->javaClassName(), "DyeAction");
	EXPECT_EQ(static_cast<const DyeAction&>(*item.getActions()[1]).getColor(), "red");
	EXPECT_EQ(item.getActions()[2]->javaClassName(), "SkillAction");
	ASSERT_NE(item.getUseAction(), nullptr);
	EXPECT_EQ(static_cast<const SkillAction&>(*item.getUseAction()).getSkillId(), 12);

	EXPECT_EQ(hookCalls, (std::vector<std::string>{"SkillAction:11", "SkillAction:12", "SkillAction:13", "ItemTemplate:1@ItemData", "ItemData:1"}))
	  << "post-order: children's hooks, then the object's own hook";
	EXPECT_TRUE(items->hookSawStaticDataRoot()) << "the holder's parent is the load context root";
}

TEST_F(BindContextTest, HooksCanBeDisabled) {
	LoadOptions options;
	options.runHooks = false;
	LoadContext context(options);
	auto items =
	  bindString<ItemData>(context, R"(<item_templates><item_template id="1" name="A"><skilllearn skill_id="2"/></item_template></item_templates>)");
	EXPECT_TRUE(hookCalls.empty());
	EXPECT_EQ(items->size(), 0u) << "index is built by the hook only";
	EXPECT_EQ(items->getItems().size(), 1u);
}

TEST_F(BindContextTest, WrappersDistinguishAbsentFromEmptyAndReplaceWhenRepeated) {
	constexpr const char* repeatedWrapper = R"(<player_data race="ELYOS">
	<properties/>
	<bonuses><stat name="a" value="1"/><stat name="b" value="2"/></bonuses>
	<bonuses><stat name="c" value="3"/></bonuses>
</player_data>)";
	LoadContext context;
	EXPECT_STATIC_DATA_ERROR(bindString<PlayerCreationData>(context, repeatedWrapper), "<memory>:4:3: player_data/bonuses (PlayerCreationData)",
	                         "Repeated element <bonuses> in <player_data>");

	LoadContext lenientContext(lenient());
	auto data = bindString<PlayerCreationData>(lenientContext, repeatedWrapper);
	ASSERT_TRUE(data->properties.has_value());
	EXPECT_TRUE(data->properties->empty()) << "present-empty wrapper";
	ASSERT_TRUE(data->bonuses.has_value());
	ASSERT_EQ(data->bonuses->size(), 1u) << "a repeated wrapper replaces the list (JAXB Lister clears the collection)";
	EXPECT_EQ((*data->bonuses)[0].name, "c");
	EXPECT_EQ(data->allies, std::nullopt);
	EXPECT_EQ(lenientContext.retiredCount(), 1u) << "the replaced list is kept alive";

	auto withValues = bindString<PlayerCreationData>(context, R"(<player_data race="ASMODIANS">
	<properties><property>gameserver.a=1</property><property/></properties>
	<allies> ELYOS
		PC_ALL </allies>
</player_data>)");
	EXPECT_EQ(withValues->properties, std::optional<std::vector<std::string>>(std::vector<std::string>{"gameserver.a=1", ""}));
	EXPECT_EQ(withValues->allies, std::optional<std::vector<Race>>(std::vector<Race>{Race::ELYOS, Race::PC_ALL})) << "@XmlList element";
	EXPECT_STATIC_DATA_ERROR(bindString<PlayerCreationData>(context, R"(<player_data race="ELYOS"><properties><stat/></properties></player_data>)"),
	                         "<memory>:1:40", "player_data/properties/stat", "Unexpected element <stat>");
	EXPECT_STATIC_DATA_ERROR(bindString<PlayerCreationData>(context, R"(<player_data race="ELYOS"><properties size="1"/></player_data>)"),
	                         "<memory>:1:39", "Unknown attribute 'size' on <properties>");
}

constexpr const char* ITEMS_XML = R"(<item_templates>
	<item_template id="100000001" name="Sword"/>
	<item_template id=" 100000002 " name="Shield"/>
	<item_template id="100000003" name="Ring"/>
</item_templates>)";

TEST_F(BindContextTest, IdRefsArePatchedAfterAllHoldersAndTypeChecked) {
	LoadContext context;
	context.addHolder("item_templates", ErasedHolder(bindString<ItemData>(context, ITEMS_XML, "items.xml")));
	auto players = bindString<PlayerInitialData>(context, R"(<player_initial_data>
	<player_data race="ELYOS" weapon="100000001" gifts=" 100000003  100000002">
		<item>100000002</item>
		<item> 100000003
		</item>
		<equipment><item>100000003</item><item>100000001</item></equipment>
	</player_data>
	<elyos_spawn_location map_id="210010000" x="1" y="2.5" z="-3"/>
</player_initial_data>)",
	                                             "players.xml");
	const PlayerCreationData& player = players->getPlayers().at(0);
	EXPECT_EQ(player.weapon, nullptr) << "not patched before resolveIdRefs";
	ASSERT_EQ(player.items.size(), 2u);
	EXPECT_EQ(context.pendingIdRefs(), 7u);
	EXPECT_EQ(players->itemsSeenByHook(), 3u) << "hooks read earlier holders";
	EXPECT_FALSE(players->idRefTaskRan());

	context.resolveIdRefs();
	const ItemData* items = context.findHolder<ItemData>();
	ASSERT_NE(items, nullptr);
	EXPECT_EQ(player.weapon, items->getItemTemplate(100000001));
	EXPECT_EQ(player.items, (std::vector<const ItemTemplate*>{items->getItemTemplate(100000002), items->getItemTemplate(100000003)}));
	EXPECT_EQ(player.gifts, (std::vector<const ItemTemplate*>{items->getItemTemplate(100000003), items->getItemTemplate(100000002)}));
	EXPECT_EQ(context.findXmlId<ItemTemplate>("100000002"), items->getItemTemplate(100000002)) << "XmlIDs are trimmed";
	EXPECT_EQ(context.findXmlId<Stat>("100000002"), nullptr) << "typed lookup";
	EXPECT_TRUE(players->idRefTaskRan()) << "runAfterIdRefResolution tasks run after patching";
	ASSERT_NE(player.gear, nullptr);
	EXPECT_EQ(player.gear->getNames(), "Ring;Sword;") << "class-level adapter initialized after IDREF resolution";
	EXPECT_EQ(context.pendingIdRefs(), 0u);
	EXPECT_EQ(players->getElyosSpawn()->y, 2.5f);
}

TEST_F(BindContextTest, UnresolvedAndWrongTypedIdRefsAreListedWithLocations) {
	LoadContext context;
	context.addHolder("item_templates", ErasedHolder(bindString<ItemData>(context, ITEMS_XML, "items.xml")));
	Stat notAnItem;
	context.registerXmlId("555", notAnItem);
	auto players = bindString<PlayerInitialData>(context, R"(<player_initial_data>
	<player_data race="ELYOS" weapon="999">
		<item>555</item>
	</player_data>
	<elyos_spawn_location/>
</player_initial_data>)",
	                                             "players.xml");
	EXPECT_STATIC_DATA_ERROR(context.resolveIdRefs(), "2 unresolved IDREF(s)", "players.xml:2:28: Unresolved IDREF '999'",
	                         "players.xml:3:4: IDREF '555' refers to a", "expected");
	EXPECT_FALSE(players->idRefTaskRan()) << "tasks do not run after a failed resolution";
}

TEST_F(BindContextTest, FailedBindingRollsBackItsXmlIdsIdRefsAndTasks) {
	LoadContext context;
	context.addHolder("item_templates", ErasedHolder(bindString<ItemData>(context, ITEMS_XML, "items.xml")));
	EXPECT_EQ(context.xmlIdCount(), 3u);
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, R"(<item_templates>
	<item_template id="9" name="kept only until the error"/>
	<item_template id="10" name="B" level="x"/>
</item_templates>)"),
	                         "Not a valid byte value: 'x'");
	EXPECT_EQ(context.xmlIdCount(), 3u) << "ids of the destroyed holder are gone";
	EXPECT_EQ(context.findXmlId<ItemTemplate>("9"), nullptr);

	EXPECT_STATIC_DATA_ERROR(bindString<PlayerInitialData>(context, R"(<player_initial_data>
	<player_data race="ELYOS" weapon="100000001"><item>100000002</item></player_data>
</player_initial_data>)"),
	                         "Missing required element <elyos_spawn_location>");
	EXPECT_EQ(context.pendingIdRefs(), 0u) << "IDREF slots of the destroyed holder are gone";
	auto players = bindString<PlayerInitialData>(context, R"(<player_initial_data>
	<player_data race="ELYOS" weapon="100000001"/><elyos_spawn_location/>
</player_initial_data>)");
	EXPECT_EQ(context.pendingIdRefs(), 1u);
	EXPECT_NO_THROW(context.resolveIdRefs());
	EXPECT_TRUE(players->idRefTaskRan()) << "only the task of the successful binding ran";
}

TEST_F(BindContextTest, ObjectsReplacedByRepeatedElementsStayAliveForTheirIdsIdRefsAndTasks) {
	constexpr const char* loadoutXml = R"(<loadout>
	<item_template id="200" name="first"/>
	<player_data race="ELYOS" weapon="100000001"><item>100000002</item><equipment><item>100000003</item></equipment><equipment><item>100000001</item></equipment></player_data>
	<reserves><player_data race="ELYOS" weapon="100000002"><equipment><item>100000001</item></equipment></player_data></reserves>
	<item_template id="201" name="second"/>
	<player_data race="ASMODIANS" weapon="100000003"/>
	<reserves><player_data race="ASMODIANS" weapon="100000003"/></reserves>
</loadout>)";
	{
		LoadContext context;
		context.addHolder("item_templates", ErasedHolder(bindString<ItemData>(context, ITEMS_XML, "items.xml")));
		EXPECT_STATIC_DATA_ERROR(bindString<Loadout>(context, loadoutXml), "Repeated element <equipment> in <player_data> (PlayerCreationData)");
		EXPECT_EQ(context.pendingIdRefs(), 0u);
		EXPECT_EQ(context.xmlIdCount(), 3u);
		EXPECT_EQ(context.retiredCount(), 0u);
	}
	LoadContext context(lenient());
	context.addHolder("item_templates", ErasedHolder(bindString<ItemData>(context, ITEMS_XML, "items.xml")));
	auto loadout = bindString<Loadout>(context, loadoutXml);
	ASSERT_EQ(context.retiredCount(), 4u) << "the first Gear (class-level adapter, replaceSingle), item_template, player_data and reserves list";
	EXPECT_EQ(loadout->item->getName(), "second");
	EXPECT_EQ(loadout->player->race, Race::ASMODIANS);
	ASSERT_EQ(loadout->reserves->size(), 1u);
	EXPECT_EQ(context.pendingIdRefs(), 8u) << "the IDREF slots of the replaced objects are still registered";
	ASSERT_NE(context.findXmlId<ItemTemplate>("200"), nullptr);
	EXPECT_EQ(context.findXmlId<ItemTemplate>("200")->getName(), "first") << "the XmlID of a replaced object still points to a live object";

	context.resolveIdRefs(); // patches slots inside the retired objects and runs their Gear tasks: must not touch freed memory
	const ItemData* items = context.findHolder<ItemData>();
	EXPECT_EQ(loadout->player->weapon, items->getItemTemplate(100000003));
	EXPECT_EQ((*loadout->reserves)[0].weapon, items->getItemTemplate(100000003));

	std::vector<ErasedHolder> retired = context.takeRetired();
	ASSERT_EQ(retired.size(), 4u);
	EXPECT_EQ(context.retiredCount(), 0u);
	std::unique_ptr<runtime::Ref<Gear>> firstGear = retired[0].take<runtime::Ref<Gear>>();
	ASSERT_NE(*firstGear, nullptr) << "the retired reference keeps the replaced RefCounted gear";
	EXPECT_EQ((*firstGear)->getNames(), "Ring;") << "the after-IDREF task of a retired object ran on the live object";
	EXPECT_EQ(retired[1].take<ItemTemplate>()->getName(), "first");
	std::unique_ptr<PlayerCreationData> firstPlayer = retired[2].take<PlayerCreationData>();
	EXPECT_EQ(firstPlayer->weapon, items->getItemTemplate(100000001)) << "retired objects are patched like live ones";
	EXPECT_EQ(firstPlayer->items, (std::vector<const ItemTemplate*>{items->getItemTemplate(100000002)}));
	ASSERT_NE(firstPlayer->gear, nullptr);
	EXPECT_EQ(firstPlayer->gear->getNames(), "Sword;") << "the last <equipment> wins";
	std::unique_ptr<std::vector<PlayerCreationData>> firstReserves = retired[3].take<std::vector<PlayerCreationData>>();
	ASSERT_EQ(firstReserves->size(), 1u);
	EXPECT_EQ((*firstReserves)[0].weapon, items->getItemTemplate(100000002));
	EXPECT_EQ((*firstReserves)[0].gear->getNames(), "Sword;");
}

TEST_F(BindContextTest, DuplicateXmlIdsAreErrorsInStrictModeAndLastWinsOtherwise) {
	constexpr const char* duplicates = R"(<item_templates>
	<item_template id="7" name="first"/>
	<item_template id="7" name="second"/>
</item_templates>)";
	{
		LoadContext context;
		EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, duplicates, "dup.xml"), "dup.xml:3:17: item_templates/item_template@id (ItemTemplate)",
		                         "Duplicate XmlID '7'", "first declared at dup.xml:2:17");
	}
	LoadContext context(lenient());
	auto items = bindString<ItemData>(context, duplicates, "dup.xml");
	EXPECT_EQ(context.findXmlId<ItemTemplate>("7"), &items->getItems()[1]);
}

TEST_F(BindContextTest, RequiredAttributesAndElements) {
	LoadContext context(lenient());
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<item_templates>\n  <item_template id=\"1\"/>\n</item_templates>", "req.xml"),
	                         "req.xml:2:4: item_templates/item_template (ItemTemplate): Missing required attribute 'name'");
	EXPECT_STATIC_DATA_ERROR(bindString<PlayerInitialData>(context, "<player_initial_data><player_data race=\"ELYOS\"/></player_initial_data>"),
	                         "player_initial_data (PlayerInitialData): Missing required element <elyos_spawn_location>");
	EXPECT_STATIC_DATA_ERROR(bindString<Weapon>(context, "<weapon min_damage=\"1\"/>"), "Missing required attribute 'max_damage'");
}

TEST_F(BindContextTest, ConversionErrorsCarryFileLineColumnAndPath) {
	LoadContext context(lenient());
	EXPECT_STATIC_DATA_ERROR(
	  bindString<ItemData>(context, "<item_templates>\n\t<item_template id=\"1\" name=\"A\" level=\"200\"/>\n</item_templates>", "items.xml"),
	  "items.xml:2:33: item_templates/item_template@level (ItemTemplate): Value out of range for byte: '200'");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\" race=\"elyos\"/></item_templates>"),
	                         "<memory>:1:48: item_templates/item_template@race (ItemTemplate): Unknown Race constant 'elyos'");
	EXPECT_STATIC_DATA_ERROR(
	  bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\">\n<cooldown>1x</cooldown></item_template></item_templates>"),
	  "<memory>:2:2: item_templates/item_template/cooldown (ItemTemplate): Not a valid int value: '1x'");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\" restrict=\"1  2\"/></item_templates>"),
	                         "item_templates/item_template@restrict (ItemTemplate)", "space separated bytes '1  2'");
	EXPECT_STATIC_DATA_ERROR(
	  bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\" start=\"2016-02-30T00:00\"/></item_templates>"),
	  "@start (ItemTemplate)", "Invalid date");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\" level=\"-128\"/></item_templates>"),
	                         "<memory>:1:18: item_templates/item_template (ItemTemplate): level -128 is reserved"); // LoadContext::fail from a hook
}

TEST_F(BindContextTest, UnknownElementsAreErrorsInBothModes) {
	for (bool strict : {true, false}) {
		LoadOptions options;
		options.strict = strict;
		LoadContext context(options);
		EXPECT_STATIC_DATA_ERROR(
		  bindString<ItemData>(context, "<item_templates>\n <item_template id=\"1\" name=\"A\"><blade/></item_template>\n</item_templates>"),
		  "<memory>:2:34: item_templates/item_template/blade (ItemTemplate): Unexpected element <blade> in <item_template>");
		EXPECT_STATIC_DATA_ERROR(
		  bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\"><desc>a<b/></desc></item_template></item_templates>"),
		  "Unexpected element <b> in text element <desc>");
		EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<item_templates><weapon min_damage=\"1\" max_damage=\"1\"/></item_templates>"),
		                         "Unexpected element <weapon> in <item_templates> (ItemData)");
	}
}

TEST_F(BindContextTest, StrictModeRejectsUnknownAttributesTextAndRepeatedElements) {
	LoadContext context;
	EXPECT_STATIC_DATA_ERROR(
	  bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\" colour=\"red\"/></item_templates>"),
	  "<memory>:1:48: item_templates/item_template@colour (ItemTemplate): Unknown attribute 'colour' on <item_template> (ItemTemplate)");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\">oops</item_template></item_templates>"),
	                         "<memory>:1:48: item_templates/item_template/text() (ItemTemplate): Unexpected text 'oops' in <item_template>");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, R"(<item_templates><item_template id="1" name="A">
	<weapon min_damage="1" max_damage="2"/><weapon min_damage="3" max_damage="4"/></item_template></item_templates>)"),
	                         "<memory>:2:42: item_templates/item_template/weapon (ItemTemplate): Repeated element <weapon>");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, R"(<item_templates><item_template id="1" name="A">
	<use_skill skill_id="1"/><use_skill skill_id="2"/></item_template></item_templates>)"),
	                         "Repeated element <use_skill>");
	EXPECT_STATIC_DATA_ERROR(
	  bindString<ItemData>(context, "<item_templates><item_template id=\"1\" name=\"A\"><desc lang=\"en\">x</desc></item_template></item_templates>"),
	  "item_templates/item_template/desc@lang (ItemTemplate): Unknown attribute 'lang' on <desc>");
}

TEST_F(BindContextTest, LenientModeWarnsCountsAndContinuesLikeJaxb) {
	LoadContext context(lenient());
	auto items = bindString<ItemData>(context, R"(<item_templates>
	<item_template id="1" name="A" colour="red">oops<weapon min_damage="1" max_damage="2"/><weapon min_damage="3" max_damage="4"/></item_template>
	<item_template id="2" name="B" colour="blue"/>
</item_templates>)");
	ASSERT_EQ(items->getItems().size(), 2u);
	EXPECT_EQ(items->getItems()[0].getWeapon()->minDamage, 3) << "the last repeated element wins";
	const BindStats& stats = context.stats();
	EXPECT_EQ(stats.attribute("item_template", "colour"), (NodeCounts{0, 0, 2}));
	EXPECT_EQ(stats.unknownText("item_template"), 1u);
}

TEST_F(BindContextTest, NamespaceAndSchemaInstanceAttributesAreIgnored) {
	LoadContext context(strictWithStats());
	auto items =
	  bindString<ItemData>(context, R"(<item_templates xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="items.xsd">
	<item_template xmlns:xs="http://www.w3.org/2001/XMLSchema-instance" xs:type="x" id="1" name="A"/>
</item_templates>)");
	EXPECT_EQ(items->getItems().size(), 1u);
	const BindStats& stats = context.stats();
	EXPECT_EQ(stats.attribute("item_templates", "xsi:noNamespaceSchemaLocation"), NodeCounts{}) << "never counted per tag (oracle totals)";
	EXPECT_EQ(stats.attribute("item_templates", "xmlns:xsi"), NodeCounts{});
	EXPECT_EQ(stats.attribute("item_template", "xs:type"), NodeCounts{});
	EXPECT_EQ(stats.namespaceAttributes("xsi:noNamespaceSchemaLocation"), 1u);
	EXPECT_EQ(stats.namespaceAttributes("xs:type"), 1u);
	EXPECT_EQ(stats.namespaceDeclarations(), 2u);
	EXPECT_EQ(stats.totalAttributes(), (NodeCounts{2, 0, 0}));
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, R"(<item_templates xmlns:foo="urn:foo" foo:bar="1"/>)"), "Unknown attribute 'foo:bar'");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, R"(<item_templates xmlns="urn:items"/>)"), "Namespaced elements are not supported");
	EXPECT_NO_THROW(bindString<ItemData>(context, R"(<item_templates xmlns=""/>)"));
}

TEST_F(BindContextTest, StatsCountEveryElementAndAttributeOnce) {
	LoadContext context(strictWithStats());
	auto data = bindString<PlayerCreationData>(context, R"(<player_data race="ELYOS" weapon="1">
	<item>2</item>
	<properties><property>a</property><property>b</property></properties>
	<allies>ELYOS</allies>
</player_data>)");
	auto items = bindString<ItemData>(context, R"(<item_templates version="1">
	<item_template id="1" name="A"><comment a="1" b="2"><deep c="3"/></comment><stat name="x"/></item_template>
</item_templates>)");
	const BindStats& stats = context.stats();
	EXPECT_EQ(stats.element("player_data"), (NodeCounts{1, 0, 0}));
	EXPECT_EQ(stats.element("item"), (NodeCounts{1, 0, 0}));
	EXPECT_EQ(stats.element("properties"), (NodeCounts{1, 0, 0}));
	EXPECT_EQ(stats.element("property"), (NodeCounts{2, 0, 0}));
	EXPECT_EQ(stats.element("comment"), (NodeCounts{0, 1, 0}));
	EXPECT_EQ(stats.element("deep"), (NodeCounts{0, 1, 0})) << "ignored subtrees are counted with their descendants";
	EXPECT_EQ(stats.attribute("deep", "c"), (NodeCounts{0, 1, 0}));
	EXPECT_EQ(stats.attribute("player_data", "weapon"), (NodeCounts{1, 0, 0}));
	EXPECT_EQ(stats.totalElements(), (NodeCounts{9, 2, 0}));
	EXPECT_EQ(stats.totalAttributes(), (NodeCounts{6, 3, 0}));

	std::ostringstream report;
	stats.write(report);
	EXPECT_EQ(report.str().substr(0, report.str().find('\n', report.str().find("attribute\tcomment\tb")) + 1), "element\tallies\t1\t0\t0\n"
	                                                                                                           "element\tcomment\t0\t1\t0\n"
	                                                                                                           "attribute\tcomment\ta\t0\t1\t0\n"
	                                                                                                           "attribute\tcomment\tb\t0\t1\t0\n");

	BindStats merged;
	merged.merge(stats);
	merged.merge(stats);
	EXPECT_EQ(merged.totalElements(), (NodeCounts{18, 4, 0}));
	merged.clear();
	EXPECT_TRUE(merged.empty());
	static_cast<void>(data);
	static_cast<void>(items);
}

TEST_F(BindContextTest, DeliberatelyIgnoredAttributesAreCountedAsIgnored) {
	LoadContext context(strictWithStats());
	auto items = bindString<ItemData>(context, R"(<item_templates><item_template id="1" name="A" c_name="x"/><item_template id="2" name="B" c_name=""/></item_templates>)");
	EXPECT_EQ(items->getItems().size(), 2u);
	EXPECT_EQ(context.stats().attribute("item_template", "c_name"), (NodeCounts{0, 2, 0}));
	EXPECT_EQ(context.stats().attribute("item_template", "name"), (NodeCounts{2, 0, 0})) << "the ignore mark does not leak to later attributes";
}

TEST_F(BindContextTest, TotalsDocumentHasTheOracleFormat) {
	LoadContext context(lenient());
	auto items = bindString<ItemData>(context, R"(<item_templates version="1" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="a.xsd">
	<item_template id="1" name="A" c_name="x" colour="red"><comment a="1"/><stat name="x"/></item_template>
	<item_template id="2" name="B"/>
</item_templates>)");
	context.stats().rootSkipped("C:/data/static_data/items/b.xml", "item_templates", {"xsi:noNamespaceSchemaLocation", "version"});
	context.stats().rootSkipped("other/c \"q\".xml", "item_templates", {});
	std::ostringstream totals;
	context.stats().writeTotals(totals, "C:\\data\\static_data");
	EXPECT_EQ(totals.str(), R"({
	"format": "aion-staticdata-totals",
	"version": 1,
	"elements": 5,
	"attributes": 8,
	"byTag": {
		"comment": {
			"count": 1,
			"attributes": {
				"a": 1
			}
		},
		"item_template": {
			"count": 2,
			"attributes": {
				"c_name": 1,
				"id": 2,
				"name": 2
			}
		},
		"item_templates": {
			"count": 1,
			"attributes": {
				"version": 1
			}
		},
		"stat": {
			"count": 1,
			"attributes": {
				"name": 1
			}
		}
	},
	"namespaceAttributes": {
		"xsi:noNamespaceSchemaLocation": 1
	},
	"skippedRoots": [
		{
			"file": "items/b.xml",
			"tag": "item_templates",
			"attributes": [
				"version",
				"xsi:noNamespaceSchemaLocation"
			]
		},
		{
			"file": "other/c \"q\".xml",
			"tag": "item_templates",
			"attributes": []
		}
	],
	"unknown": {
		"attributes": 1,
		"text": 0
	},
	"namespaceDeclarations": 1
}
)") << "unknown attributes (colour) are left out of byTag, so a lenient load that dropped data differs from the oracle";

	std::ostringstream empty;
	BindStats().writeTotals(empty);
	EXPECT_EQ(empty.str(), "{\n\t\"format\": \"aion-staticdata-totals\",\n\t\"version\": 1,\n\t\"elements\": 0,\n\t\"attributes\": 0,\n\t\"byTag\": {},\n"
	                       "\t\"namespaceAttributes\": {},\n\t\"skippedRoots\": [],\n\t\"unknown\": {\n\t\t\"attributes\": 0,\n\t\t\"text\": 0\n\t},\n"
	                       "\t\"namespaceDeclarations\": 0\n}\n");
	static_cast<void>(items);
}

TEST_F(BindContextTest, ParseErrorsAndEncodings) {
	LoadContext context;
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<item_templates>\n  <item_template id=\"1\">\n</item_templates>", "bad.xml"),
	                         "bad.xml:3:", "XML parse error");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "", "empty.xml"), "empty.xml");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><item_templates/>"), "only UTF-8");
	EXPECT_STATIC_DATA_ERROR(bindString<ItemData>(context, std::string_view("\xFF\xFE<\0a\0/\0>\0", 10)), "only UTF-8");
	EXPECT_NO_THROW(bindString<ItemData>(context, "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!-- c --><item_templates/>"));
	auto document = XmlDocument::parseString("\xEF\xBB\xBF<a>\r\n <b/>\n</a>");
	EXPECT_EQ(document->describe(document->locate(document->root().first_child())), "<memory>:2:3");
}

// ---- binder bugs are reported, not silently tolerated -------------------------------------------------------------------------------------------

struct SloppyHolder {
	std::vector<Stat> stats;
};
struct UnreservedHolder {
	std::vector<Stat> stats;
};
struct MisplacedIgnoreHolder {};

} // namespace
} // namespace aion::gameserver::xml::test

namespace aion::gameserver::xml {
template <>
struct XmlBinding<test::SloppyHolder> {
	static bool element(test::SloppyHolder&, BindContext&, pugi::xml_node, std::string_view n) { return n == "stat"; }
};
template <>
struct XmlBinding<test::UnreservedHolder> {
	static bool element(test::UnreservedHolder& o, BindContext& c, pugi::xml_node e, std::string_view) {
		c.bindList(o.stats, e);
		return true;
	}
};
template <>
struct XmlBinding<test::MisplacedIgnoreHolder> {
	static bool element(test::MisplacedIgnoreHolder&, BindContext& c, pugi::xml_node e, std::string_view) {
		c.ignoreAttribute(); // only valid inside attribute()
		c.ignoreElement(e);
		return true;
	}
};
} // namespace aion::gameserver::xml

namespace aion::gameserver::xml::test {
namespace {

TEST_F(BindContextTest, BinderBugsThrowIllegalState) {
	LoadContext context;
	try {
		bindString<SloppyHolder>(context, "<h><stat/></h>");
		FAIL() << "expected IllegalStateException";
	} catch (const commons::utils::IllegalStateException& e) {
		EXPECT_NE(std::string(e.what()).find("without consuming it"), std::string::npos) << e.what();
	}
	try {
		bindString<UnreservedHolder>(context, "<h><stat name=\"a\"/></h>");
		FAIL() << "expected IllegalStateException";
	} catch (const commons::utils::IllegalStateException& e) {
		EXPECT_NE(std::string(e.what()).find("is not reserved"), std::string::npos) << e.what();
	}
	try {
		bindString<MisplacedIgnoreHolder>(context, "<h><x/></h>");
		FAIL() << "expected IllegalStateException";
	} catch (const commons::utils::IllegalStateException& e) {
		EXPECT_NE(std::string(e.what()).find("ignoreAttribute called outside"), std::string::npos) << e.what();
	}
}

} // namespace
} // namespace aion::gameserver::xml::test
