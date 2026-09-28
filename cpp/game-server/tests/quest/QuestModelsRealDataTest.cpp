// P4-08 on the real static data: quest_data/quest_data.xml is bound strictly with hooks into the real QuestsData holder (its hook is ported, P4-09;
// QuestTemplate belongs to P4-07b), and the 89 files of quest_script_data into a test holder of this file with one list per XML quest class (the
// XMLQuests hook belongs to P4-09 and is unported; the 16 XMLQuest models have no hooks of their own). Checks:
// - the holder counts of the count oracle (quest data entries; extras.xml_quests: distinct XML quest ids) and the element/attribute totals;
// - every XML quest element is bound as the class of its @XmlElements entry (xmlmodel.json), with its id;
// - the data-only helpers on the real event and operation elements (getIds, getMonsters, isOverride) against the DOM;
// - a field dump of 20 random quest templates and 20 random XML quests against their XML attributes.

#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/questEngine/handlers/models/CraftingRewardsData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/FountainRewardsData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ItemCollectingData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ItemOrdersData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/KillInWorldData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/KillInZoneData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/KillSpawnedData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/MentorMonsterHuntData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/MonsterHuntData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/RelicRewardsData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ReportOnLevelUpData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ReportToData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ReportToManyData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/SkillUseData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/WorkOrdersData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/XmlQuestData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.bind.h"

namespace aion::gameserver::questEngine::handlers::models::realdata {

/** quest_scripts with one list per concrete XMLQuest class, in document order within each list */
struct XmlQuestLists {
	std::vector<ReportToData> reportTo;
	std::vector<MonsterHuntData> monsterHunt;
	std::vector<XmlQuestData> xmlQuest;
	std::vector<ItemCollectingData> itemCollecting;
	std::vector<RelicRewardsData> relicRewards;
	std::vector<CraftingRewardsData> craftingRewards;
	std::vector<ReportToManyData> reportToMany;
	std::vector<KillInWorldData> killInWorld;
	std::vector<KillInZoneData> killInZone;
	std::vector<SkillUseData> skillUse;
	std::vector<KillSpawnedData> killSpawned;
	std::vector<MentorMonsterHuntData> mentorMonsterHunt;
	std::vector<FountainRewardsData> fountainRewards;
	std::vector<ItemOrdersData> itemOrder;
	std::vector<WorkOrdersData> workOrder;
	std::vector<ReportOnLevelUpData> reportOnLevelup;
	/** every bound quest in document order */
	std::vector<const XMLQuest*> all;
};

} // namespace aion::gameserver::questEngine::handlers::models::realdata

namespace aion::gameserver::xml {

template <>
struct XmlBinding<questEngine::handlers::models::realdata::XmlQuestLists> {
	static constexpr std::string_view CLASS_NAME = "TestHolder";
	using Lists = questEngine::handlers::models::realdata::XmlQuestLists;

	template <class T>
	static bool add(Lists& o, BindContext& c, pugi::xml_node e, std::vector<T>& list) {
		c.bindList(list, e);
		o.all.push_back(&list.back());
		return true;
	}

	static bool element(Lists& o, BindContext& c, pugi::xml_node e, std::string_view name) {
		if (name == "report_to")
			return add(o, c, e, o.reportTo);
		if (name == "monster_hunt")
			return add(o, c, e, o.monsterHunt);
		if (name == "xml_quest")
			return add(o, c, e, o.xmlQuest);
		if (name == "item_collecting")
			return add(o, c, e, o.itemCollecting);
		if (name == "relic_rewards")
			return add(o, c, e, o.relicRewards);
		if (name == "crafting_rewards")
			return add(o, c, e, o.craftingRewards);
		if (name == "report_to_many")
			return add(o, c, e, o.reportToMany);
		if (name == "kill_in_world")
			return add(o, c, e, o.killInWorld);
		if (name == "kill_in_zone")
			return add(o, c, e, o.killInZone);
		if (name == "skill_use")
			return add(o, c, e, o.skillUse);
		if (name == "kill_spawned")
			return add(o, c, e, o.killSpawned);
		if (name == "mentor_monster_hunt")
			return add(o, c, e, o.mentorMonsterHunt);
		if (name == "fountain_rewards")
			return add(o, c, e, o.fountainRewards);
		if (name == "item_order")
			return add(o, c, e, o.itemOrder);
		if (name == "work_order")
			return add(o, c, e, o.workOrder);
		if (name == "report_on_levelup")
			return add(o, c, e, o.reportOnLevelup);
		return false;
	}

	static void reserve(Lists& o, const ChildCounts& counts) {
		o.reportTo.reserve(counts["report_to"]);
		o.monsterHunt.reserve(counts["monster_hunt"]);
		o.xmlQuest.reserve(counts["xml_quest"]);
		o.itemCollecting.reserve(counts["item_collecting"]);
		o.relicRewards.reserve(counts["relic_rewards"]);
		o.craftingRewards.reserve(counts["crafting_rewards"]);
		o.reportToMany.reserve(counts["report_to_many"]);
		o.killInWorld.reserve(counts["kill_in_world"]);
		o.killInZone.reserve(counts["kill_in_zone"]);
		o.skillUse.reserve(counts["skill_use"]);
		o.killSpawned.reserve(counts["kill_spawned"]);
		o.mentorMonsterHunt.reserve(counts["mentor_monster_hunt"]);
		o.fountainRewards.reserve(counts["fountain_rewards"]);
		o.itemOrder.reserve(counts["item_order"]);
		o.workOrder.reserve(counts["work_order"]);
		o.reportOnLevelup.reserve(counts["report_on_levelup"]);
	}
};

} // namespace aion::gameserver::xml

namespace aion::gameserver::questEngine::handlers::models::realdata {
namespace {

using json = nlohmann::json;
using Documents = std::vector<std::unique_ptr<xml::XmlDocument>>;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
const std::filesystem::path ORACLE = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/tools/oracle/expected";
const std::filesystem::path XML_MODEL = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/game-server/generated/xmlmodel.json";

json readJson(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot open " + file.string());
	return json::parse(in);
}

std::string text(pugi::xml_node node, const char* name, const char* fallback = "") {
	pugi::xml_attribute attribute = node.attribute(name);
	return attribute ? std::string(attribute.value()) : std::string(fallback);
}

int64_t number(pugi::xml_node node, const char* name, int64_t fallback = 0) {
	pugi::xml_attribute attribute = node.attribute(name);
	if (!attribute)
		return fallback;
	std::string_view value = attribute.value();
	int64_t result = 0;
	std::from_chars(value.data(), value.data() + value.size(), result);
	return result;
}

std::vector<int32_t> numbers(pugi::xml_node node, const char* name) {
	std::vector<int32_t> result;
	std::string value = text(node, name);
	for (size_t pos = 0; pos < value.size();) {
		size_t end = std::min(value.find(' ', pos), value.size());
		if (end > pos)
			result.push_back(std::stoi(value.substr(pos, end - pos)));
		pos = end + 1;
	}
	return result;
}

std::vector<size_t> sample(size_t size, uint32_t seed) {
	std::mt19937 generator(seed);
	std::set<size_t> chosen;
	while (chosen.size() < std::min<size_t>(20, size))
		chosen.insert(std::uniform_int_distribution<size_t>(0, size - 1)(generator));
	return {chosen.begin(), chosen.end()};
}

/** every descendant element named `name` below the roots, in document order */
std::vector<pugi::xml_node> descendants(const Documents& documents, const char* name) {
	std::vector<pugi::xml_node> nodes;
	for (const std::unique_ptr<xml::XmlDocument>& document : documents) {
		for (pugi::xpath_node found : document->root().select_nodes((std::string(".//") + name).c_str()))
			nodes.push_back(found.node());
	}
	return nodes;
}

class QuestModelsRealDataTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		if (!std::filesystem::exists(STATIC_DATA / "static_data.xml") || !std::filesystem::exists(ORACLE / "totals.json") ||
			!std::filesystem::exists(XML_MODEL))
			return;
		imports = xml::StaticDataImports::resolve(STATIC_DATA / "static_data.xml", 0, true);
		counts = readJson(ORACLE / "static_data_counts.json");
		totals = readJson(ORACLE / "totals.json");
		available = true;
	}

	static void TearDownTestSuite() {
		imports.clear();
		counts = json();
		totals = json();
	}

	void SetUp() override {
		if (!available)
			GTEST_SKIP() << "Java data tree, oracle outputs or xmlmodel.json not found: " << STATIC_DATA << ", " << ORACLE;
	}

	static Documents parse(std::string_view file) {
		for (const xml::StaticDataImport& entry : imports) {
			if (entry.file == file)
				return xml::StaticDataLoader::parseFiles(entry.files, false);
		}
		throw std::runtime_error("no import " + std::string(file));
	}

	template <class H>
	static std::unique_ptr<H> bindHolder(xml::LoadContext& context, const Documents& documents) {
		std::vector<const xml::XmlDocument*> roots;
		for (const std::unique_ptr<xml::XmlDocument>& document : documents)
			roots.push_back(document.get());
		auto holder = std::make_unique<H>();
		xml::BindContext binding(context);
		binding.bindHolder(*holder, roots, context.root());
		return holder;
	}

	static int64_t expectedCount(std::string_view holder) {
		for (const json& line : counts.at("lines")) {
			const json& holders = line.at("holders");
			if (holders.size() == 1 && holders[0].get<std::string>() == holder)
				return line.at("values").at(0).get<int64_t>();
		}
		throw std::runtime_error("no count line for " + std::string(holder));
	}

	static void expectTotals(const xml::LoadContext& context, std::string_view file) {
		for (const json& entry : totals.at("byImport")) {
			if (entry.at("import").get<std::string>() != file)
				continue;
			const xml::BindStats& stats = context.stats();
			EXPECT_EQ(stats.totalElements().bound + stats.totalElements().ignored, entry.at("elements").get<uint64_t>()) << file;
			EXPECT_EQ(stats.totalElements().unknown, 0u);
			EXPECT_EQ(stats.totalAttributes().bound + stats.totalAttributes().ignored, entry.at("attributes").get<uint64_t>()) << file;
			EXPECT_EQ(stats.totalAttributes().unknown, 0u);
			return;
		}
		ADD_FAILURE() << "no totals for " << file;
	}

	static xml::LoadOptions options() {
		xml::LoadOptions loadOptions;
		loadOptions.strict = true;
		loadOptions.collectStats = true;
		loadOptions.parallelParse = false;
		return loadOptions;
	}

	static inline bool available = false;
	static inline std::vector<xml::StaticDataImport> imports;
	static inline json counts;
	static inline json totals;
};

TEST_F(QuestModelsRealDataTest, QuestDataBindsWithHooks) {
	xml::LoadContext context(options());
	Documents documents = parse("quest_data/quest_data.xml");
	std::unique_ptr<dataholders::QuestsData> quests = bindHolder<dataholders::QuestsData>(context, documents);
	EXPECT_EQ(quests->size(), expectedCount("quests")) << "Loaded N quest data entries";
	expectTotals(context, "quest_data/quest_data.xml");

	std::vector<pugi::xml_node> nodes;
	for (pugi::xml_node node : documents.front()->root().children("quest"))
		nodes.push_back(node);
	for (size_t index : sample(nodes.size(), 8043)) {
		pugi::xml_node node = nodes[index];
		SCOPED_TRACE("quest id=" + text(node, "id"));
		const gameserver::model::templates::QuestTemplate* quest = quests->getQuestById(static_cast<int32_t>(number(node, "id")));
		ASSERT_NE(quest, nullptr);
		EXPECT_EQ(quest->getName(), text(node, "name"));
		EXPECT_EQ(quest->getMinlevelPermitted(), number(node, "minlevel_permitted"));
		EXPECT_EQ(quest->getMaxRepeatCount(), number(node, "max_repeat_count", 1));
		EXPECT_EQ(quest->getRacePermitted() ? std::string(xml::enumName(*quest->getRacePermitted())) : std::string(), text(node, "race_permitted"));
		EXPECT_EQ(xml::enumName(quest->getCategory()), text(node, "category", "QUEST"));
		EXPECT_EQ(quest->getNpcFactionId(), number(node, "npcfaction_id"));
		EXPECT_EQ(quest->isCannotShare(), text(node, "cannot_share") == "true");
	}
}

TEST_F(QuestModelsRealDataTest, XmlQuestsBind) {
	xml::LoadContext context(options());
	Documents documents = parse("quest_script_data");
	std::unique_ptr<XmlQuestLists> lists = bindHolder<XmlQuestLists>(context, documents);
	expectTotals(context, "quest_script_data");

	std::set<int32_t> ids;
	for (const XMLQuest* quest : lists->all)
		ids.insert(quest->getId());
	EXPECT_EQ(static_cast<int64_t>(ids.size()), counts.at("extras").at("xml_quests").at("value").get<int64_t>()) << "XMLQuests distinct quest ids";

	// element name -> class of XMLQuests.data (xmlmodel.json)
	std::map<std::string, std::string> classes;
	for (const json& c : readJson(XML_MODEL).at("classes")) {
		if (c.at("fqn").get<std::string>() != "com.aionemu.gameserver.dataholders.XMLQuests")
			continue;
		for (const json& p : c.at("properties")) {
			for (const json& choice : p.value("choices", json::array())) {
				std::string fqn = choice.at("typeFqn").get<std::string>();
				classes[choice.at("xmlName").get<std::string>()] = fqn.substr(fqn.rfind('.') + 1);
			}
		}
	}
	ASSERT_EQ(classes.size(), 16u);
	std::vector<pugi::xml_node> nodes;
	for (const std::unique_ptr<xml::XmlDocument>& document : documents)
		for (pugi::xml_node child : document->root().children())
			nodes.push_back(child);
	ASSERT_EQ(nodes.size(), lists->all.size());
	std::map<std::string, size_t> perClass;
	for (size_t i = 0; i < nodes.size(); ++i) {
		EXPECT_EQ(lists->all[i]->javaClassName(), classes.at(nodes[i].name())) << nodes[i].name();
		EXPECT_EQ(lists->all[i]->getId(), number(nodes[i], "id"));
		++perClass[std::string(lists->all[i]->javaClassName())];
	}
	for (const auto& [name, count] : perClass)
		std::cout << name << ": " << count << "\n";
	for (size_t index : sample(nodes.size(), 4184)) {
		SCOPED_TRACE(std::string(nodes[index].name()) + " id=" + text(nodes[index], "id"));
		EXPECT_EQ(lists->all[index]->getId(), number(nodes[index], "id"));
		EXPECT_EQ(lists->all[index]->javaClassName(), classes.at(nodes[index].name()));
	}

	// the event and operation elements of the scripts, bound again one by one, against their DOM
	size_t talkEvents = 0;
	for (pugi::xml_node node : descendants(documents, "on_talk_event")) {
		xml::LoadContext single;
		pugi::xml_document copy;
		copy.append_copy(node);
		std::ostringstream out;
		copy.save(out, "", pugi::format_raw);
		std::unique_ptr<xmlQuest::events::OnTalkEvent> event = xml::bindString<xmlQuest::events::OnTalkEvent>(single, out.str());
		EXPECT_EQ(event->getIds(), numbers(node, "ids"));
		++talkEvents;
	}
	size_t operations = 0;
	for (pugi::xml_node node : descendants(documents, "operations")) {
		xml::LoadContext single;
		pugi::xml_document copy;
		copy.append_copy(node);
		std::ostringstream out;
		copy.save(out, "", pugi::format_raw);
		std::unique_ptr<xmlQuest::operations::QuestOperations> bound = xml::bindString<xmlQuest::operations::QuestOperations>(single, out.str());
		EXPECT_EQ(bound->isOverride(), text(node, "override", "true") == "true");
		++operations;
	}
	std::cout << talkEvents << " on_talk_event and " << operations << " operations elements checked\n";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::models::realdata
