// P4-08 on the real static data: skills/skill_templates.xml is bound strictly with every hook into the real SkillData holder (its hook is ported,
// P4-09), skills/motion_times.xml and skills/signet_data_templates.xml into test holders of this file (the MotionData hook belongs to P4-09 and is
// unported). Checks:
// - the holder counts of the count oracle (tools/oracle/expected/static_data_counts.json) and the element/attribute totals per import;
// - the Effects hook on every skill against the DOM: effect types (element name -> class from xmlmodel.json -> Java's EffectType rule), the
//   conflict types and the noresist normalization (ALWAYS_NO_RESIST copied from Effects.java); the SkillTemplate helpers on every skill;
// - the MotionTime hook: every times element is found by getTimesFor with its race, gender and weapon (the last duplicate wins);
// - a field dump of 20 random skill templates against their XML attributes.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/skillengine/condition/ChainCondition.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/model/Motion.h"
#include "aion/gameserver/skillengine/model/MotionTime.bind.h"
#include "aion/gameserver/skillengine/model/MotionTime.h"
#include "aion/gameserver/skillengine/model/SignetDataTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SignetDataTemplate.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/Properties.h"

namespace aion::gameserver::skillengine::realdata {

struct MotionTimes {
	std::vector<model::MotionTime> list;
};
struct SignetDataTemplates {
	std::vector<model::SignetDataTemplate> list;
};

} // namespace aion::gameserver::skillengine::realdata

namespace aion::gameserver::xml {

/** XmlBinding of a test holder: the children named `ELEMENT` are bound in place into `list` */
template <class H, const char* ELEMENT>
struct SkillShellsTestHolderBinding {
	static constexpr std::string_view CLASS_NAME = "TestHolder";
	static bool element(H& o, BindContext& c, pugi::xml_node e, std::string_view name) {
		if (name != ELEMENT)
			return false;
		c.bindList(o.list, e);
		return true;
	}
	static void reserve(H& o, const ChildCounts& counts) { o.list.reserve(counts[ELEMENT]); }
};

inline constexpr char MOTION_TIME[] = "motion_time";
inline constexpr char SIGNET_DATA_TEMPLATE[] = "signet_data_template";

template <>
struct XmlBinding<skillengine::realdata::MotionTimes> : SkillShellsTestHolderBinding<skillengine::realdata::MotionTimes, MOTION_TIME> {};
template <>
struct XmlBinding<skillengine::realdata::SignetDataTemplates>
	: SkillShellsTestHolderBinding<skillengine::realdata::SignetDataTemplates, SIGNET_DATA_TEMPLATE> {};

} // namespace aion::gameserver::xml

namespace aion::gameserver::skillengine::realdata {
namespace {

using json = nlohmann::json;
using Documents = std::vector<std::unique_ptr<xml::XmlDocument>>;
using effect::EffectType;
using gameserver::model::Gender;
using gameserver::model::Race;
using gameserver::model::templates::item::enums::ItemGroup;

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

float decimal(pugi::xml_node node, const char* name, float fallback = 0.0f) {
	pugi::xml_attribute attribute = node.attribute(name);
	if (!attribute)
		return fallback;
	std::string_view value = attribute.value();
	float result = 0.0f;
	std::from_chars(value.data(), value.data() + value.size(), result);
	return result;
}

bool flag(pugi::xml_node node, const char* name) {
	std::string value = text(node, name, "false");
	return value == "true" || value == "1";
}

std::vector<pugi::xml_node> children(const Documents& documents, const char* name) {
	std::vector<pugi::xml_node> nodes;
	for (const std::unique_ptr<xml::XmlDocument>& document : documents)
		for (pugi::xml_node child : document->root().children(name))
			nodes.push_back(child);
	return nodes;
}

std::vector<size_t> sample(size_t size, uint32_t seed) {
	std::mt19937 generator(seed);
	std::set<size_t> chosen;
	while (chosen.size() < std::min<size_t>(20, size))
		chosen.insert(std::uniform_int_distribution<size_t>(0, size - 1)(generator));
	return {chosen.begin(), chosen.end()};
}

EffectType effectType(std::string_view name) {
	std::optional<EffectType> type = xml::enumFromName<EffectType>(name);
	if (!type)
		throw std::runtime_error("no EffectType " + std::string(name));
	return *type;
}

class SkillShellsRealDataTest : public testing::Test {
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

	/** element name -> Java simple class name of an @XmlElements property (xmlmodel.json) */
	static std::map<std::string, std::string> choiceClasses(std::string_view classFqn, std::string_view property) {
		json model = readJson(XML_MODEL);
		std::map<std::string, std::string> result;
		for (const json& c : model.at("classes")) {
			if (c.at("fqn").get<std::string>() != classFqn)
				continue;
			for (const json& p : c.at("properties")) {
				if (p.at("javaName").get<std::string>() != property)
					continue;
				for (const json& choice : p.at("choices")) {
					std::string fqn = choice.at("typeFqn").get<std::string>();
					result[choice.at("xmlName").get<std::string>()] = fqn.substr(fqn.rfind('.') + 1);
				}
			}
		}
		return result;
	}

	static inline bool available = false;
	static inline std::vector<xml::StaticDataImport> imports;
	static inline json counts;
	static inline json totals;
};

// ---- skill templates -------------------------------------------------------------------------------------------------------------------------------

TEST_F(SkillShellsRealDataTest, SkillTemplatesBindWithHooks) {
	auto start = std::chrono::steady_clock::now();
	xml::LoadContext context(options());
	Documents documents = parse("skills/skill_templates.xml");
	std::unique_ptr<dataholders::SkillData> data = bindHolder<dataholders::SkillData>(context, documents);
	std::cout << "real skill templates bound in "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms\n";
	std::vector<const model::SkillTemplate*> templates = data->getSkillTemplates();
	EXPECT_EQ(static_cast<int64_t>(templates.size()), expectedCount("skill_data")) << "Loaded N skill templates";
	expectTotals(context, "skills/skill_templates.xml");

	std::map<int32_t, const model::SkillTemplate*> byId;
	for (const model::SkillTemplate* skill : templates)
		byId[skill->getSkillId()] = skill;
	std::vector<pugi::xml_node> nodes = children(documents, "skill_template");
	ASSERT_EQ(nodes.size(), byId.size()) << "skill ids are unique in the data";

	const std::map<std::string, std::string> effectClasses =
		choiceClasses("com.aionemu.gameserver.skillengine.effect.Effects", "effects");
	ASSERT_EQ(effectClasses.size(), 170u);
	// Effects.java: CONFLICT_TYPES and ALWAYS_NO_RESIST, copied by hand
	std::set<EffectType> conflictTypes;
	for (const char* name : {"SHIELD", "PROTECT", "REFLECTOR", "MPSHIELD"})
		conflictTypes.insert(effectType(name));
	std::set<EffectType> alwaysNoResist;
	for (const char* name : {"ABSOLUTESTATTOPCBUFF", "ALWAYSBLOCK", "ALWAYSDODGE", "ALWAYSPARRY", "ALWAYSRESIST", "ARMORMASTERY", "APBOOST", "AURA",
			 "BOOSTHATE", "BOOSTHEAL", "BOOSTSKILLCASTINGTIME", "BOOSTSKILLCOST", "BOOSTSPELLATTACK", "CASEHEAL", "CHANGEHATEONATTACKED",
			 "CONDSKILLLAUNCHER", "CONVERTHEAL", "DISPELDEBUFF", "DISPELDEBUFFMENTAL", "DISPELDEBUFFPHYSICAL", "DISPELNPCDEBUFF", "DPHEAL",
			 "DPHEALINSTANT", "DPTRANSFER", "DRBOOST", "ESCAPE", "EXTENDAURARANGE", "FPHEAL", "FPHEALINSTANT", "HEAL", "HEALINSTANT", "HIDE", "HIPASS",
			 "HOSTILEUP", "INVULNERABLEWING", "SKILLXPBOOST", "MPHEAL", "MPHEALINSTANT", "MPSHIELD", "NODEATHPENALTY", "NORESURRECTPENALTY",
			 "ONETIMEBOOSTHEAL", "ONETIMEBOOSTSKILLATTACK", "ONETIMEBOOSTSKILLCRITICAL", "PETORDERUSEULTRASKILL", "POLYMORPH", "PROCDPHEALINSTANT",
			 "PROCFPHEALINSTANT", "PROCHEALINSTANT", "PROCMPHEALINSTANT", "PROCVPHEALINSTANT", "PROTECT", "RANDOMMOVELOC", "REBIRTH", "RECALLINSTANT",
			 "REFLECTOR", "RESURRECT", "RESURRECTBASE", "RESURRECTPOSITIONAL", "RETURN", "RETURNPOINT", "RIDEROBOT", "SANCTUARY", "SEARCH",
			 "SHAPECHANGE", "SHIELD", "SHIELDMASTERY", "SIGNET", "SKILLLAUNCHER", "STATBOOST", "STATUP", "SUBTYPEBOOSTRESIST",
			 "SUBTYPEEXTENDDURATION", "SUMMON", "SUMMONBINDINGGROUPGATE", "SUMMONFUNCTIONALNPC", "SUMMONGROUPGATE", "SUMMONHOMING", "SUMMONHOUSEGATE",
			 "SUMMONSERVANT", "SUMMONSKILLAREA", "SUMMONTOTEM", "SUMMONTRAP", "SUPPORTEVENT", "SWITCHHOSTILE", "SWITCHHPMP", "WEAPONSTATBOOST",
			 "WEAPONSTATUP", "WEAPONDUAL", "WEAPONMASTERY", "XPBOOST"})
		alwaysNoResist.insert(effectType(name));
	const size_t effectTypeCount = xml::EnumTraits<EffectType>::names.size();

	size_t effectsChecked = 0;
	size_t normalized = 0;
	size_t multiCast = 0;
	std::set<EffectType> typesInData;
	for (pugi::xml_node node : nodes) {
		const model::SkillTemplate& skill = *byId.at(static_cast<int32_t>(number(node, "skill_id")));
		SCOPED_TRACE("skill_template skill_id=" + text(node, "skill_id"));
		pugi::xml_node effectsNode = node.child("effects");
		ASSERT_EQ(skill.getEffects() != nullptr, static_cast<bool>(effectsNode));
		std::set<EffectType> types;
		int32_t position = 0;
		for (pugi::xml_node effectNode : effectsNode.children()) {
			++position;
			const std::string& className = effectClasses.at(effectNode.name());
			std::string constant;
			for (size_t pos = 0; pos < className.size();) {
				if (className.compare(pos, 6, "Effect") == 0)
					pos += 6;
				else
					constant += static_cast<char>(std::toupper(static_cast<unsigned char>(className[pos++])));
			}
			EffectType type = effectType(constant);
			types.insert(type);
			const effect::EffectTemplate* effect = skill.getEffectTemplate(position);
			ASSERT_NE(effect, nullptr);
			EXPECT_EQ(effect->javaClassName(), className);
			bool expectedNoResist = flag(effectNode, "noresist") || alwaysNoResist.contains(type) ||
				(className == "SkillAttackInstantEffect" && flag(effectNode, "cannotmiss"));
			EXPECT_EQ(effect->isNoResist(), expectedNoResist) << className;
			normalized += alwaysNoResist.contains(type) && !flag(effectNode, "noresist") ? 1 : 0;
			++effectsChecked;
		}
		EXPECT_EQ(skill.getEffectTemplate(position + 1), nullptr);
		if (skill.getEffects() != nullptr) {
			for (size_t ordinal = 0; ordinal < effectTypeCount; ++ordinal) {
				EffectType type = static_cast<EffectType>(ordinal);
				ASSERT_EQ(skill.getEffects()->hasAnyEffectType({type}), types.contains(type)) << xml::enumName(type);
			}
			std::set<EffectType> conflicts;
			std::ranges::set_intersection(types, conflictTypes, std::inserter(conflicts, conflicts.begin()));
			EXPECT_EQ(skill.getEffects()->getPossibleConflictEffectTypes(), conflicts);
		}
		typesInData.insert(types.begin(), types.end());
		EXPECT_EQ(skill.hasResurrectEffect(), types.contains(EffectType::RESURRECT) || types.contains(EffectType::RESURRECTPOSITIONAL));
		EXPECT_EQ(skill.hasEvadeEffect(), types.contains(EffectType::EVADE));
		EXPECT_EQ(skill.hasRecallInstant(), types.contains(EffectType::RECALLINSTANT));

		// SkillTemplate helpers
		std::string activation = text(node, "activation");
		EXPECT_EQ(skill.isPassive(), activation == "PASSIVE");
		EXPECT_EQ(skill.isToggle(), activation == "TOGGLE");
		EXPECT_EQ(skill.isProvoked(), activation == "PROVOKED");
		EXPECT_EQ(skill.isMaintain(), activation == "MAINTAIN");
		EXPECT_EQ(skill.isActive(), activation == "ACTIVE");
		EXPECT_EQ(skill.isCharge(), activation == "CHARGE");
		int64_t cooldownId = number(node, "cooldownId");
		EXPECT_EQ(skill.getCooldownId(), cooldownId > 0 ? cooldownId : number(node, "skill_id"));
		pugi::xml_node chain = node.child("startconditions").child("chain");
		EXPECT_EQ(skill.getChainCondition() != nullptr, static_cast<bool>(chain));
		EXPECT_EQ(skill.isMultiCast(), chain && number(chain, "selfcount", 1) > 1);
		multiCast += skill.isMultiCast() ? 1 : 0;
		EXPECT_EQ(skill.getSkillChargeCondition() != nullptr, static_cast<bool>(node.child("startconditions").child("skillcharge")));
		EXPECT_EQ(skill.getRideRobotCondition() != nullptr, static_cast<bool>(node.child("useconditions").child("ride_robot")));
		if (node.child("startconditions")) {
			EXPECT_EQ(skill.getHpCondition() != nullptr, static_cast<bool>(node.child("startconditions").child("hp")));
			EXPECT_EQ(skill.getMovedCondition() != nullptr, static_cast<bool>(node.child("startconditions").child("move_casting")));
		}
	}
	std::cout << effectsChecked << " effects checked, " << normalized << " normalized to noresist, " << typesInData.size()
			  << " effect types in the data, " << multiCast << " multi-cast skills\n";
	EXPECT_GT(normalized, 0u);
	EXPECT_GT(multiCast, 0u);

	// field dump of 20 random skill templates
	for (size_t index : sample(nodes.size(), 13570)) {
		pugi::xml_node node = nodes[index];
		const model::SkillTemplate& skill = *byId.at(static_cast<int32_t>(number(node, "skill_id")));
		SCOPED_TRACE("skill_template skill_id=" + text(node, "skill_id"));
		EXPECT_EQ(skill.getName(), text(node, "name"));
		EXPECT_EQ(skill.getL10nId(), number(node, "nameId"));
		EXPECT_EQ(skill.getStack(), text(node, "stack"));
		EXPECT_EQ(skill.getGroup(), text(node, "group"));
		EXPECT_EQ(skill.getLvl(), number(node, "lvl"));
		EXPECT_EQ(xml::enumName(skill.getType()), text(node, "skilltype"));
		EXPECT_EQ(xml::enumName(skill.getSubType()), text(node, "skillsubtype"));
		EXPECT_EQ(xml::enumName(skill.getActivationAttribute()), text(node, "activation"));
		EXPECT_EQ(skill.getTargetSlot() ? std::string(xml::enumName(*skill.getTargetSlot())) : std::string(), text(node, "tslot"));
		EXPECT_EQ(xml::enumName(skill.getDispelCategory()), text(node, "dispel_category", "NONE"));
		EXPECT_EQ(skill.getReqDispelLevel(), number(node, "req_dispel_level"));
		EXPECT_EQ(skill.getDuration(), number(node, "duration"));
		EXPECT_EQ(skill.getCooldown(), number(node, "cooldown"));
		EXPECT_EQ(skill.getCancelRate(), number(node, "cancel_rate"));
		EXPECT_EQ(skill.getChainSkillProb(), number(node, "chain_skill_prob", 100));
		EXPECT_EQ(xml::enumName(skill.getHostileType()), text(node, "hostile_type", "NONE"));
		EXPECT_EQ(xml::enumName(skill.getStigmaType()), text(node, "stigma", "NONE"));
		pugi::xml_node motion = node.child("motion");
		ASSERT_EQ(skill.getMotion() != nullptr, static_cast<bool>(motion));
		if (motion) {
			EXPECT_EQ(skill.getMotion()->getName(), text(motion, "name"));
			EXPECT_EQ(skill.getMotion()->getSpeed(), number(motion, "speed", 100));
		}
		pugi::xml_node properties = node.child("properties");
		ASSERT_EQ(skill.getProperties() != nullptr, static_cast<bool>(properties));
		if (properties) {
			EXPECT_EQ(xml::enumName(skill.getProperties()->getFirstTarget()), text(properties, "first_target"));
			EXPECT_EQ(skill.getProperties()->getFirstTargetRange(), number(properties, "first_target_range"));
		}
		int32_t position = 0;
		for (pugi::xml_node effectNode : node.child("effects").children()) {
			const effect::EffectTemplate* effect = skill.getEffectTemplate(++position);
			ASSERT_NE(effect, nullptr);
			EXPECT_EQ(effect->getDuration2(),
				number(effectNode, "duration2") + (dynamic_cast<const effect::AbstractOverTimeEffect*>(effect) != nullptr ? 1000 : 0))
				<< "AbstractOverTimeEffect adds 1000";
			EXPECT_EQ(effect->getEffectId(), number(effectNode, "effectid"));
			EXPECT_EQ(effect->getPosition(), number(effectNode, "e"));
			EXPECT_EQ(effect->getBasicLvl(), number(effectNode, "basiclvl"));
			EXPECT_EQ(effect->getChange().size(), static_cast<size_t>(std::distance(effectNode.children("change").begin(),
				effectNode.children("change").end())));
			EXPECT_EQ(effect->getSubEffect() != nullptr, static_cast<bool>(effectNode.child("subeffect")));
		}
	}
}

// ---- motion times ------------------------------------------------------------------------------------------------------------------------------

TEST_F(SkillShellsRealDataTest, MotionTimesBindWithHooks) {
	xml::LoadContext context(options());
	Documents documents = parse("skills/motion_times.xml");
	std::unique_ptr<MotionTimes> motions = bindHolder<MotionTimes>(context, documents);
	std::set<std::string> names;
	for (const model::MotionTime& motion : motions->list)
		names.insert(motion.getName());
	EXPECT_EQ(static_cast<int64_t>(names.size()), expectedCount("motion_times")) << "Loaded N motion times (MotionData: a map by name)";
	expectTotals(context, "skills/motion_times.xml");

	// MotionTime.parseTimesFrom, hand-derived: weapon -> the WeaponTypeWrapper keys (main hand, off hand) it is stored under
	using Hands = std::pair<std::optional<ItemGroup>, std::optional<ItemGroup>>;
	const std::map<std::string, std::vector<Hands>> weaponKeys = {{"1hand", {{ItemGroup::SWORD, std::nullopt}}},
		{"2hand", {{ItemGroup::GREATSWORD, std::nullopt}}}, {"keyblade", {{ItemGroup::KEYBLADE, std::nullopt}}},
		{"polearm", {{ItemGroup::POLEARM, std::nullopt}}}, {"dagger", {{ItemGroup::DAGGER, std::nullopt}}}, {"mace", {{ItemGroup::MACE, std::nullopt}}},
		{"staff", {{ItemGroup::STAFF, std::nullopt}}},
		{"2weapon", {{ItemGroup::DAGGER, ItemGroup::DAGGER}, {ItemGroup::SWORD, ItemGroup::SWORD}, {ItemGroup::MACE, ItemGroup::MACE}}},
		{"noweapon", {{std::nullopt, std::nullopt}}}, {"book", {{ItemGroup::SPELLBOOK, std::nullopt}}}, {"orb", {{ItemGroup::ORB, std::nullopt}}},
		{"1gun", {{ItemGroup::GUN, std::nullopt}}}, {"2gun", {{ItemGroup::GUN, ItemGroup::GUN}}}, {"cannon", {{ItemGroup::CANNON, std::nullopt}}},
		{"bow", {{ItemGroup::BOW, std::nullopt}}}, {"harp", {{ItemGroup::HARP, std::nullopt}}}};
	const std::vector<std::tuple<const char*, Race, Gender>> lists = {{"asmodian_female", Race::ASMODIANS, Gender::FEMALE},
		{"asmodian_male", Race::ASMODIANS, Gender::MALE}, {"elyos_female", Race::ELYOS, Gender::FEMALE}, {"elyos_male", Race::ELYOS, Gender::MALE}};

	std::vector<pugi::xml_node> nodes = children(documents, "motion_time");
	ASSERT_EQ(nodes.size(), motions->list.size());
	size_t lookups = 0;
	for (size_t m = 0; m < nodes.size(); ++m) {
		const model::MotionTime& motion = motions->list[m];
		SCOPED_TRACE("motion_time name=" + text(nodes[m], "name"));
		for (const auto& [list, race, gender] : lists) {
			// (key, id) -> the last times element stored under it
			std::map<std::pair<Hands, int64_t>, pugi::xml_node> expected;
			for (pugi::xml_node times : nodes[m].children(list)) {
				for (const Hands& hands : weaponKeys.at(text(times, "weapon")))
					expected[{hands, number(times, "id")}] = times;
			}
			for (const auto& [key, times] : expected) {
				const model::Times* found = motion.getTimesFor(false, race, gender, key.first.first, key.first.second, static_cast<int32_t>(key.second));
				ASSERT_NE(found, nullptr) << list << " " << text(times, "weapon") << " id " << key.second;
				EXPECT_EQ(found->getId(), key.second);
				EXPECT_FLOAT_EQ(found->getMinTime(), decimal(times, "min"));
				EXPECT_FLOAT_EQ(found->getMaxTime(), decimal(times, "max"));
				EXPECT_FLOAT_EQ(found->getAnimationLength(), decimal(times, "animation_length"));
				++lookups;
			}
		}
		std::map<int64_t, pugi::xml_node> robot;
		for (pugi::xml_node times : nodes[m].children("robot"))
			robot[number(times, "id")] = times;
		EXPECT_EQ(motion.robotTimes.size(), robot.size());
		for (const auto& [id, times] : robot) {
			const model::Times* found = motion.getTimesFor(true, Race::ELYOS, Gender::MALE, std::nullopt, std::nullopt, static_cast<int32_t>(id));
			ASSERT_NE(found, nullptr);
			EXPECT_FLOAT_EQ(found->getAnimationLength(), decimal(times, "animation_length"));
			++lookups;
		}
	}
	std::cout << lookups << " motion time lookups checked\n";
	EXPECT_GT(lookups, 9000u);
}

// ---- signet data -------------------------------------------------------------------------------------------------------------------------------

TEST_F(SkillShellsRealDataTest, SignetDataTemplatesBind) {
	xml::LoadContext context(options());
	Documents documents = parse("skills/signet_data_templates.xml");
	std::unique_ptr<SignetDataTemplates> signets = bindHolder<SignetDataTemplates>(context, documents);
	EXPECT_EQ(static_cast<int64_t>(signets->list.size()), expectedCount("signet_data_templates"));
	expectTotals(context, "skills/signet_data_templates.xml");
	std::vector<pugi::xml_node> nodes = children(documents, "signet_data_template");
	ASSERT_EQ(nodes.size(), signets->list.size());
	for (size_t i = 0; i < nodes.size(); ++i) {
		for (pugi::xml_node data : nodes[i].children("signet_data")) {
			const model::SignetData* found = signets->list[i].getSignetDataForSignetLevel(static_cast<int32_t>(number(data, "lvl")));
			ASSERT_NE(found, nullptr);
			EXPECT_FLOAT_EQ(found->getDamageMultiplier(), decimal(data, "dmg_multi"));
			EXPECT_EQ(found->getAddEffectProb(), number(data, "add_effect_prob", 1)) << "Java: = 1 (required, but absent in the data)";
		}
		EXPECT_EQ(signets->list[i].getSignetDataForSignetLevel(99), nullptr);
	}
}

} // namespace
} // namespace aion::gameserver::skillengine::realdata
