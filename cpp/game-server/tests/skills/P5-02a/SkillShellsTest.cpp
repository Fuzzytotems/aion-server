// P4-08 (the lease over the skillengine xmlgen shells): the afterUnmarshal hooks of Effects, Motion, Times and MotionTime and the data-only helpers
// of SkillTemplate, Conditions, SignetDataTemplate, SkillLearnTemplate and SkillChargeCondition, bound from fixture XML. Expectations are derived by
// hand from Effects.java, Motion.java, Times.java, MotionTime.java, WeaponTypeWrapper.java, SkillTemplate.java, SignetDataTemplate.java and
// SkillLearnTemplate.java. Test doubles: SkillData bound from XML text and published into DataManager for the lookups that read SKILL_DATA; a
// Player (the real class with stat container doubles, as in the P4-13 item tests) for MotionTime.getTimesFor(Player, id).

#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/skillengine/condition/ChainCondition.h"
#include "aion/gameserver/skillengine/condition/HpCondition.h"
#include "aion/gameserver/skillengine/condition/PlayerMovedCondition.h"
#include "aion/gameserver/skillengine/condition/RideRobotCondition.h"
#include "aion/gameserver/skillengine/condition/SkillChargeCondition.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.bind.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/model/Motion.bind.h"
#include "aion/gameserver/skillengine/model/MotionTime.bind.h"
#include "aion/gameserver/skillengine/model/MotionTime.h"
#include "aion/gameserver/skillengine/model/SignetDataTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SignetDataTemplate.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/Times.bind.h"
#include "aion/gameserver/skillengine/model/Times.h"

namespace aion::gameserver::skillengine {
namespace {

using effect::EffectType;
using gameserver::model::Gender;
using gameserver::model::Race;
using gameserver::model::templates::item::enums::ItemGroup;

template <class T>
std::unique_ptr<T> bindXml(const std::string& text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

/** Publishes a SkillData into DataManager::SKILL_DATA and forgets it again when the scope ends, also when an ASSERT ends the test early */
class PublishedSkillData {
public:
	explicit PublishedSkillData(std::unique_ptr<dataholders::SkillData> data) { dataholders::DataManager::SKILL_DATA.publish(std::move(data)); }
	~PublishedSkillData() { dataholders::DataManager::SKILL_DATA.resetForTests(); }
	PublishedSkillData(const PublishedSkillData&) = delete;
	PublishedSkillData& operator=(const PublishedSkillData&) = delete;
};

std::string skillXml(int32_t skillId, const std::string& attributes, const std::string& children = {}) {
	return "<skill_template skill_id=\"" + std::to_string(skillId) + "\" name=\"s" + std::to_string(skillId) +
		R"(" nameId="1" stack="S" skilltype="MAGICAL" skillsubtype="BUFF" duration="0" )" + attributes + ">" + children + "</skill_template>";
}

// ---- Effects ------------------------------------------------------------------------------------------------------------------------------------

TEST(EffectsHookTest, ResolvesTypesConflictsAndNoResist) {
	std::unique_ptr<effect::Effects> effects = bindXml<effect::Effects>(R"(<effects>
		<root duration2="1" e="1"/>
		<shield duration2="1" e="2" noresist="false"/>
		<protect duration2="1" e="3"/>
		<healinstant duration2="1" e="4"/>
		<stun duration2="1" e="5" noresist="true"/>
		<skillatk duration2="1" e="6" cannotmiss="true"/>
	</effects>)");
	const auto& list = effects->getEffects();
	ASSERT_EQ(list.size(), 6u);
	EXPECT_EQ(effects->getPossibleConflictEffectTypes(), (std::set<EffectType>{EffectType::PROTECT, EffectType::SHIELD}))
		<< "CONFLICT_TYPES are SHIELD, PROTECT, REFLECTOR and MPSHIELD";
	EXPECT_TRUE(effects->hasAnyEffectType({EffectType::ROOT}));
	EXPECT_TRUE(effects->hasAnyEffectType({EffectType::BLIND, EffectType::HEALINSTANT}));
	EXPECT_TRUE(effects->hasAnyEffectType({EffectType::SKILLATTACKINSTANT})) << "SkillAttackInstantEffect -> SKILLATTACKINSTANT";
	EXPECT_FALSE(effects->hasAnyEffectType({EffectType::BLIND, EffectType::MPSHIELD}));
	EXPECT_FALSE(effects->hasAnyEffectType({}));
	EXPECT_FALSE(list[0]->isNoResist()) << "ROOT is not in ALWAYS_NO_RESIST";
	EXPECT_TRUE(list[1]->isNoResist()) << "SHIELD is normalized to noresist, overriding noresist=\"false\"";
	EXPECT_TRUE(list[2]->isNoResist()) << "PROTECT";
	EXPECT_TRUE(list[3]->isNoResist()) << "HEALINSTANT";
	EXPECT_TRUE(list[4]->isNoResist()) << "STUN keeps the XML value";
	EXPECT_TRUE(list[5]->isNoResist()) << "SkillAttackInstantEffect: cannotmiss";
}

TEST(EffectsHookTest, WithoutEffectsJavaThrowsNullPointerException) {
	EXPECT_THROW(bindXml<effect::Effects>("<effects/>"), std::exception);
	EXPECT_NO_THROW(bindXml<model::SkillTemplate>(skillXml(1, R"(activation="ACTIVE")"))) << "no <effects> element: no hook runs";
}

/**
 * Every element of Effects' @XmlElements list binds and its class resolves to an EffectType (Java: IllegalArgumentException "Missing EffectType").
 * The element names, classes and required attributes come from xmlmodel.json; the expected constant is derived here from the Java class name.
 */
TEST(EffectsHookTest, EveryEffectChoiceResolvesItsEffectType) {
	const std::filesystem::path model = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/game-server/generated/xmlmodel.json";
	std::ifstream in(model, std::ios::binary);
	if (!in)
		GTEST_SKIP() << "no " << model;
	nlohmann::json json = nlohmann::json::parse(in);
	std::map<std::string, const nlohmann::json*> classes;
	for (const nlohmann::json& c : json.at("classes"))
		classes[c.at("fqn").get<std::string>()] = &c;
	const nlohmann::json* effectsClass = classes.at("com.aionemu.gameserver.skillengine.effect.Effects");
	const nlohmann::json* choices = nullptr;
	for (const nlohmann::json& property : effectsClass->at("properties")) {
		if (property.at("javaName") == "effects")
			choices = &property.at("choices");
	}
	ASSERT_NE(choices, nullptr);
	size_t checked = 0;
	for (const nlohmann::json& choice : *choices) {
		std::string fqn = choice.at("typeFqn").get<std::string>();
		std::string element = choice.at("xmlName").get<std::string>();
		if (classes.at(fqn)->value("abstract", false)) {
			// JAXB cannot instantiate an abstract class (BufEffect): the factory leaves the element out, so it is an unknown element
			EXPECT_THROW(bindXml<effect::Effects>("<effects><" + element + " duration2=\"1\"/></effects>"), std::exception) << element;
			continue;
		}
		std::string xmlText = "<effects><" + element;
		for (std::string current = fqn; classes.contains(current);) {
			const nlohmann::json& c = *classes.at(current);
			for (const nlohmann::json& property : c.at("properties")) {
				if (property.at("required").get<bool>() && property.at("node") == "attribute")
					xmlText += " " + property.at("xmlName").get<std::string>() + (property.at("javaType") == "String" ? "=\"x\"" : "=\"1\"");
			}
			current = c.contains("superclass") && c.at("superclass").is_string() ? c.at("superclass").get<std::string>() : std::string();
		}
		xmlText += "/></effects>";
		std::string simpleName = fqn.substr(fqn.rfind('.') + 1);
		std::string constant;
		for (size_t pos = 0; pos < simpleName.size();) {
			if (simpleName.compare(pos, 6, "Effect") == 0) {
				pos += 6;
			} else {
				constant += static_cast<char>(std::toupper(static_cast<unsigned char>(simpleName[pos++])));
			}
		}
		std::optional<EffectType> expected = xml::enumFromName<EffectType>(constant);
		ASSERT_TRUE(expected.has_value()) << simpleName;
		std::unique_ptr<effect::Effects> effects;
		ASSERT_NO_THROW(effects = bindXml<effect::Effects>(xmlText)) << xmlText;
		EXPECT_EQ(effects->getEffects().at(0)->javaClassName(), simpleName);
		EXPECT_TRUE(effects->hasAnyEffectType({*expected})) << element;
		++checked;
	}
	EXPECT_EQ(checked, 169u) << "170 choices, of which BufEffect is abstract";
}

// ---- Motion, Times, MotionTime -----------------------------------------------------------------------------------------------------------------

TEST(MotionHookTest, KeepsTheName) {
	EXPECT_EQ(bindXml<model::Motion>(R"(<motion name="phburst" speed="85"/>)")->getName(), "phburst");
	std::unique_ptr<model::Motion> instant = bindXml<model::Motion>(R"(<motion instant_skill="true"/>)");
	EXPECT_EQ(instant->getName(), "") << "Java null";
	EXPECT_EQ(instant->getSpeed(), 100);
	EXPECT_TRUE(instant->isInstantSkill());
}

TEST(TimesHookTest, WeaponIsRequiredByTheHook) {
	std::unique_ptr<model::Times> times = bindXml<model::Times>(R"(<times weapon="1hand" id="2" min="0.5" max="0.75" animation_length="1.25"/>)");
	EXPECT_EQ(times->getWeapon(), "1hand");
	EXPECT_EQ(times->getId(), 2);
	EXPECT_FLOAT_EQ(times->getMaxTime(), 0.75f);
	EXPECT_THROW(bindXml<model::Times>(R"(<times id="1"/>)"), std::exception) << "Java: weapon.intern() on null";
}

TEST(MotionTimeHookTest, WeaponTypeWrapperNormalizesDualWielding) {
	using Key = std::pair<std::optional<ItemGroup>, std::optional<ItemGroup>>;
	EXPECT_EQ(model::MotionTime::weaponTypeWrapper(ItemGroup::DAGGER, ItemGroup::SWORD), (Key{ItemGroup::DAGGER, ItemGroup::DAGGER}));
	EXPECT_EQ(model::MotionTime::weaponTypeWrapper(ItemGroup::SWORD, ItemGroup::MACE), (Key{ItemGroup::SWORD, ItemGroup::SWORD}));
	EXPECT_EQ(model::MotionTime::weaponTypeWrapper(ItemGroup::GUN, ItemGroup::GUN), (Key{ItemGroup::GUN, ItemGroup::GUN}));
	EXPECT_EQ(model::MotionTime::weaponTypeWrapper(ItemGroup::TOOLHOES, ItemGroup::SWORD), (Key{ItemGroup::TOOLHOES, ItemGroup::TOOLHOES}));
	EXPECT_EQ(model::MotionTime::weaponTypeWrapper(ItemGroup::GREATSWORD, ItemGroup::SWORD), (Key{ItemGroup::GREATSWORD, std::nullopt}))
		<< "other main hands drop the off hand";
	EXPECT_EQ(model::MotionTime::weaponTypeWrapper(ItemGroup::STAFF, std::nullopt), (Key{ItemGroup::STAFF, std::nullopt}));
	EXPECT_EQ(model::MotionTime::weaponTypeWrapper(std::nullopt, ItemGroup::SWORD), (Key{std::nullopt, ItemGroup::SWORD}))
		<< "only both hands set are normalized";
}

TEST(MotionTimeHookTest, TimesForRaceGenderWeaponAndRobot) {
	std::unique_ptr<model::MotionTime> motion = bindXml<model::MotionTime>(R"(<motion_time name="attack">
		<asmodian_female weapon="1hand" id="1" min="0.1"/>
		<asmodian_female weapon="1hand" id="3" min="0.3"/>
		<asmodian_female weapon="2weapon" id="1" min="0.21"/>
		<asmodian_male weapon="noweapon" id="1" min="0.4"/>
		<elyos_female weapon="2gun" id="2" min="0.5"/>
		<elyos_male weapon="book" id="1" min="0.6"/>
		<elyos_male weapon="book" id="1" min="0.61"/>
		<robot weapon="cannon" id="2" min="0.7"/>
	</motion_time>)");
	EXPECT_EQ(motion->getName(), "attack");
	auto minTime = [](const model::Times* times) { return times != nullptr ? times->getMinTime() : -1.0f; };
	// the highest id <= requested that the weapon map has; Java returns times.get(i) of the first found weapon map, so a gap ends the search
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ASMODIANS, Gender::FEMALE, ItemGroup::SWORD, std::nullopt, 1)), 0.1f);
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ASMODIANS, Gender::FEMALE, ItemGroup::SWORD, std::nullopt, 3)), 0.3f);
	EXPECT_EQ(motion->getTimesFor(false, Race::ASMODIANS, Gender::FEMALE, ItemGroup::SWORD, std::nullopt, 2), nullptr)
		<< "the SWORD map exists, so Java returns its get(2) == null without trying id 1";
	// 2weapon: DAGGER+DAGGER, SWORD+SWORD and MACE+MACE
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ASMODIANS, Gender::FEMALE, ItemGroup::DAGGER, ItemGroup::SWORD, 1)), 0.21f);
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ASMODIANS, Gender::FEMALE, ItemGroup::SWORD, ItemGroup::SWORD, 1)), 0.21f);
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ASMODIANS, Gender::FEMALE, ItemGroup::MACE, ItemGroup::DAGGER, 1)), 0.21f);
	EXPECT_EQ(motion->getTimesFor(false, Race::ASMODIANS, Gender::FEMALE, ItemGroup::MACE, std::nullopt, 1), nullptr) << "no 1hand mace map";
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ASMODIANS, Gender::MALE, std::nullopt, std::nullopt, 1)), 0.4f) << "noweapon";
	EXPECT_EQ(motion->getTimesFor(false, Race::ASMODIANS, Gender::MALE, std::nullopt, std::nullopt, 5), nullptr)
		<< "the noweapon map exists: Java returns its get(5) == null";
	EXPECT_EQ(motion->getTimesFor(false, Race::ASMODIANS, Gender::MALE, ItemGroup::BOW, std::nullopt, 1), nullptr);
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ELYOS, Gender::FEMALE, ItemGroup::GUN, ItemGroup::GUN, 2)), 0.5f);
	EXPECT_EQ(motion->getTimesFor(false, Race::ELYOS, Gender::FEMALE, ItemGroup::GUN, std::nullopt, 2), nullptr) << "1gun is another key";
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(false, Race::ELYOS, Gender::MALE, ItemGroup::SPELLBOOK, std::nullopt, 1)), 0.61f)
		<< "HashMap.put: the last duplicate wins";
	EXPECT_EQ(motion->getTimesFor(false, Race::NPC, Gender::MALE, ItemGroup::SPELLBOOK, std::nullopt, 1), nullptr) << "only ASMODIANS and ELYOS";
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(true, Race::ELYOS, Gender::MALE, ItemGroup::SPELLBOOK, std::nullopt, 4)), 0.7f)
		<< "robot: ids 4 and 3 are missing, id 2 found";
	EXPECT_EQ(motion->getTimesFor(true, Race::ELYOS, Gender::MALE, std::nullopt, std::nullopt, 1), nullptr);
	ASSERT_EQ(motion->robotTimes.size(), 1u);
	EXPECT_EQ(motion->getTimesFor(false, Race::ELYOS, Gender::MALE, ItemGroup::SPELLBOOK, std::nullopt, 0), nullptr) << "no id below 1";
}

// ---- MotionTime.getTimesFor(Player, id) ---------------------------------------------------------------------------------------------------------

namespace player = gameserver::model::gameobjects::player;

std::vector<runtime::Ref<player::PetCommonData>> noPets(player::Player& /*player*/) {
	return {};
}

/** CreatureGameStats double (the stat calculation belongs to P5-01) */
class TestGameStats final : public gameserver::model::stats::container::CreatureGameStats {
public:
	explicit TestGameStats(gameserver::model::gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const gameserver::model::templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<gameserver::model::stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<gameserver::model::stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<gameserver::model::stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<gameserver::model::stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

/** CreatureLifeStats double (PlayerLifeStats reads PlayerGameStats) */
class TestLifeStats final : public gameserver::model::stats::container::CreatureLifeStats {
public:
	explicit TestLifeStats(gameserver::model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

/** The real Player with the stat containers of the doubles above */
class TestPlayer final : public player::Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, gameserver::model::account::PlayerAccountData& playerAccountData, gameserver::model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override = default;

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			// PlayerGameStats(Player&) is P5-01: everything before it ran
		}
		setGameStats(std::make_unique<TestGameStats>(*this));
		setLifeStats(std::make_unique<TestLifeStats>(*this));
	}
};

class MotionTimePlayerTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 13));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
		player::PetList::setPlayerPetsLoaderForTests(&noPets);
	}

	void TearDown() override {
		player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** Java PlayerService.getPlayer: account, common data, appearance, account data; then the Player */
	static runtime::Ref<TestPlayer> createPlayer(int32_t objectId, Race race, Gender gender, runtime::Ref<gameserver::model::account::Account>& account) {
		account = gameserver::model::account::Account::create(1000 + objectId);
		runtime::Ref<player::PlayerCommonData> commonData = player::PlayerCommonData::create(objectId);
		commonData->setName("Motion" + std::to_string(objectId));
		commonData->setRace(race);
		commonData->setGender(gender);
		runtime::Ref<player::PlayerAppearance> appearance = player::PlayerAppearance::create();
		account->addPlayerAccountData(std::make_unique<gameserver::model::account::PlayerAccountData>(*account, *commonData, *appearance));
		return gameserver::model::gameobjects::VisibleObject::create<TestPlayer>(*account->getPlayerAccountData(objectId), *account);
	}

	runtime::ManualClock clock{0};
};

TEST_F(MotionTimePlayerTest, PlayerOverloadReadsRobotModeRaceGenderAndWeapons) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	std::unique_ptr<model::MotionTime> motion = bindXml<model::MotionTime>(R"(<motion_time name="attack">
		<asmodian_male weapon="noweapon" id="1" min="0.1"/>
		<elyos_female weapon="noweapon" id="1" min="0.2"/>
		<elyos_female weapon="1hand" id="1" min="0.25"/>
		<robot weapon="cannon" id="1" min="0.3"/>
	</motion_time>)");
	auto minTime = [](const model::Times* times) { return times != nullptr ? times->getMinTime() : -1.0f; };
	runtime::Ref<gameserver::model::account::Account> elyosAccount;
	runtime::Ref<TestPlayer> elyos = createPlayer(31, Race::ELYOS, Gender::FEMALE, elyosAccount);
	// no equipped weapons: the (null, null) key of "noweapon". Equipping a weapon needs the weapon skill list and, for the off hand,
	// WeaponDualEffect.hasDualWieldEffect (P5-04); the key normalization itself is covered by MotionTimeHookTest.*
	ASSERT_FALSE(elyos->getEquipment().getMainHandWeaponType().has_value());
	ASSERT_FALSE(elyos->getEquipment().getOffHandWeaponType().has_value());
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(*elyos, 2)), -1.0f) << "the noweapon map exists: Java returns its get(2) == null";
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(*elyos, 1)), 0.2f) << "elyos female without weapons: noweapon, not 1hand";
	elyos->setRobotId(5);
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(*elyos, 3)), 0.3f) << "robot mode: robot times, the highest id <= 3";
	elyos->setRobotId(0);

	runtime::Ref<gameserver::model::account::Account> asmodianAccount;
	runtime::Ref<TestPlayer> asmodian = createPlayer(32, Race::ASMODIANS, Gender::MALE, asmodianAccount);
	EXPECT_FLOAT_EQ(minTime(motion->getTimesFor(*asmodian, 1)), 0.1f) << "asmodian male map";
}

TEST(MotionTimeHookTest, UnknownWeaponIsNotImplemented) {
	EXPECT_THROW(bindXml<model::MotionTime>(R"(<motion_time name="a"><elyos_male weapon="shield" id="1"/></motion_time>)"), std::exception);
	EXPECT_NO_THROW(bindXml<model::MotionTime>(R"(<motion_time name="a"><robot weapon="shield" id="1"/></motion_time>)"))
		<< "robot times are not parsed by weapon";
}

// ---- SkillTemplate -----------------------------------------------------------------------------------------------------------------------------

TEST(SkillTemplateHelpersTest, ActivationAndCooldownId) {
	const std::vector<std::pair<std::string, int>> activations = {{"PASSIVE", 0}, {"TOGGLE", 1}, {"PROVOKED", 2}, {"MAINTAIN", 3}, {"ACTIVE", 4},
		{"CHARGE", 5}};
	for (const auto& [name, index] : activations) {
		std::unique_ptr<model::SkillTemplate> skill = bindXml<model::SkillTemplate>(skillXml(7, "activation=\"" + name + "\""));
		std::vector<bool> flags = {skill->isPassive(), skill->isToggle(), skill->isProvoked(), skill->isMaintain(), skill->isActive(), skill->isCharge()};
		for (int i = 0; i < 6; ++i)
			EXPECT_EQ(flags[i], i == index) << name << " flag " << i;
	}
	EXPECT_EQ(bindXml<model::SkillTemplate>(skillXml(7, R"(activation="ACTIVE")"))->getCooldownId(), 7) << "no cooldownId: the skill id";
	EXPECT_EQ(bindXml<model::SkillTemplate>(skillXml(7, R"(activation="ACTIVE" cooldownId="0")"))->getCooldownId(), 7);
	EXPECT_EQ(bindXml<model::SkillTemplate>(skillXml(7, R"(activation="ACTIVE" cooldownId="792")"))->getCooldownId(), 792);
}

TEST(SkillTemplateHelpersTest, EffectTemplatesByPosition) {
	std::unique_ptr<model::SkillTemplate> skill = bindXml<model::SkillTemplate>(skillXml(1, R"(activation="ACTIVE")",
		R"(<effects><root duration2="1" e="1"/><resurrect duration2="1" e="2"/></effects>)"));
	ASSERT_NE(skill->getEffectTemplate(1), nullptr);
	EXPECT_EQ(skill->getEffectTemplate(1)->javaClassName(), "RootEffect");
	EXPECT_EQ(skill->getEffectTemplate(2)->javaClassName(), "ResurrectEffect");
	EXPECT_EQ(skill->getEffectTemplate(3), nullptr);
	EXPECT_THROW(skill->getEffectTemplate(0), runtime::IndexOutOfBoundsException) << "Java: get(-1)";
	EXPECT_EQ(bindXml<model::SkillTemplate>(skillXml(2, R"(activation="ACTIVE")"))->getEffectTemplate(1), nullptr) << "no effects";
	EXPECT_EQ(bindXml<model::SkillTemplate>(skillXml(2, R"(activation="ACTIVE")"))->getEffectTemplate(0), nullptr) << "null effects: no get";

	EXPECT_TRUE(skill->hasResurrectEffect());
	EXPECT_FALSE(skill->hasEvadeEffect());
	EXPECT_FALSE(skill->hasRecallInstant());
	EXPECT_TRUE(skill->hasAnyEffect({EffectType::STUN, EffectType::ROOT}));
	EXPECT_FALSE(bindXml<model::SkillTemplate>(skillXml(2, R"(activation="ACTIVE")"))->hasAnyEffect(true, {EffectType::ROOT}))
		<< "no effects: false without reading SKILL_DATA";
}

TEST(SkillTemplateHelpersTest, HasAnyEffectOfSubEffects) {
	xml::LoadContext context;
	const model::SkillTemplate* skill10 = nullptr;
	{
		PublishedSkillData published(xml::bindString<dataholders::SkillData>(context,
			"<skill_data>" + skillXml(10, R"(activation="ACTIVE")", R"(<effects><root duration2="1" e="1"><subeffect skill_id="11"/></root></effects>)") +
				skillXml(11, R"(activation="ACTIVE")", R"(<effects><stun duration2="1" e="1"><subeffect skill_id="12"/></stun></effects>)") +
				skillXml(12, R"(activation="ACTIVE")", R"(<effects><blind duration2="1" e="1"/></effects>)") +
				skillXml(13, R"(activation="ACTIVE")", R"(<effects><root duration2="1" e="1"><subeffect skill_id="99"/></root></effects>)") +
				"</skill_data>"));
		skill10 = dataholders::DataManager::SKILL_DATA->getSkillTemplate(10);
		const model::SkillTemplate* skill13 = dataholders::DataManager::SKILL_DATA->getSkillTemplate(13);
		ASSERT_NE(skill10, nullptr);
		ASSERT_NE(skill13, nullptr);
		EXPECT_FALSE(skill10->hasAnyEffect({EffectType::STUN}));
		EXPECT_TRUE(skill10->hasAnyEffect(true, {EffectType::STUN})) << "the sub effect skill 11 has STUN";
		EXPECT_FALSE(skill10->hasAnyEffect(true, {EffectType::BLIND})) << "not recursive: skill 12 is the sub effect of the sub effect";
		EXPECT_TRUE(skill13->hasAnyEffect(true, {EffectType::ROOT})) << "own effects first";
		EXPECT_THROW(skill13->hasAnyEffect(true, {EffectType::STUN}), runtime::NullPointerException) << "Java: getSkillTemplate(99) is null";
	}
	// the holder is leaked by resetForTests, so skill10 stays valid
	EXPECT_THROW(skill10->hasAnyEffect(true, {EffectType::STUN}), runtime::NullPointerException) << "SKILL_DATA is not published";
}

TEST(SkillTemplateHelpersTest, Conditions) {
	std::unique_ptr<model::SkillTemplate> skill = bindXml<model::SkillTemplate>(skillXml(1, R"(activation="ACTIVE")",
		R"(<startconditions><hp value="10"/><chain category="C_1TH" selfcount="2"/><skillcharge value="3"/><move_casting allow="true"/></startconditions>)"
		R"(<useconditions><ride_robot/></useconditions>)"));
	ASSERT_NE(skill->getChainCondition(), nullptr);
	EXPECT_EQ(skill->getChainCondition()->getCategory(), "C_1TH");
	EXPECT_TRUE(skill->isMultiCast()) << "selfcount 2 > 1";
	ASSERT_NE(skill->getSkillChargeCondition(), nullptr);
	EXPECT_EQ(skill->getSkillChargeCondition()->getValue(), 3);
	ASSERT_NE(skill->getHpCondition(), nullptr);
	EXPECT_EQ(skill->getHpCondition()->getHpValue(), 10);
	EXPECT_NE(skill->getMovedCondition(), nullptr);
	EXPECT_NE(skill->getRideRobotCondition(), nullptr);
	ASSERT_NE(skill->getStartconditions(), nullptr);
	EXPECT_EQ(skill->getStartconditions()->getConditions().size(), 4u);

	std::unique_ptr<model::SkillTemplate> single = bindXml<model::SkillTemplate>(skillXml(2, R"(activation="ACTIVE")",
		R"(<startconditions><chain category="C_2TH"/></startconditions>)"));
	EXPECT_FALSE(single->isMultiCast()) << "selfcount defaults to 1";
	EXPECT_EQ(single->getHpCondition(), nullptr);
	EXPECT_EQ(single->getRideRobotCondition(), nullptr) << "no useconditions";

	std::unique_ptr<model::SkillTemplate> none = bindXml<model::SkillTemplate>(skillXml(3, R"(activation="ACTIVE")"));
	EXPECT_EQ(none->getChainCondition(), nullptr);
	EXPECT_FALSE(none->isMultiCast());
	EXPECT_EQ(none->getSkillChargeCondition(), nullptr);
	EXPECT_THROW(none->getHpCondition(), runtime::NullPointerException) << "Java iterates startconditions without a null check";
	EXPECT_THROW(none->getMovedCondition(), runtime::NullPointerException);
}

// ---- SignetDataTemplate, SkillLearnTemplate --------------------------------------------------------------------------------------------------------

TEST(SignetDataTemplateTest, SignetDataForLevel) {
	std::unique_ptr<model::SignetDataTemplate> signet = bindXml<model::SignetDataTemplate>(R"(<signet_data_template signet_skill="SIGNET1">
		<signet_data lvl="0" dmg_multi="0.1"/><signet_data lvl="2" dmg_multi="0.5"/><signet_data lvl="2" dmg_multi="0.7"/></signet_data_template>)");
	ASSERT_NE(signet->getSignetDataForSignetLevel(2), nullptr);
	EXPECT_FLOAT_EQ(signet->getSignetDataForSignetLevel(2)->getDamageMultiplier(), 0.5f) << "the first match";
	EXPECT_EQ(signet->getSignetDataForSignetLevel(1), nullptr);
	EXPECT_THROW(bindXml<model::SignetDataTemplate>(R"(<signet_data_template signet_skill="SIGNET1"/>)")->getSignetDataForSignetLevel(0),
		runtime::NullPointerException);
}

TEST(SkillLearnTemplateTest, StigmaFlagsAndSkillLevel) {
	auto learn = [](const std::string& stigma) {
		return bindXml<model::SkillLearnTemplate>(R"(<skill skillId="20" minLevel="1")" + stigma + "/>");
	};
	EXPECT_FALSE(learn("")->isStigma());
	EXPECT_TRUE(learn(R"( stigma="1")")->isStigma());
	EXPECT_FALSE(learn(R"( stigma="1")")->isLinkedStigma());
	EXPECT_TRUE(learn(R"( stigma="4")")->isLinkedStigma());
	EXPECT_FALSE(learn(R"( stigma="-1")")->isStigma());

	EXPECT_THROW(learn("")->getSkillLevel(), runtime::NullPointerException) << "SKILL_DATA is not published";
	xml::LoadContext context;
	PublishedSkillData published(
		xml::bindString<dataholders::SkillData>(context, "<skill_data>" + skillXml(20, R"(activation="ACTIVE" lvl="3")") + "</skill_data>"));
	EXPECT_EQ(learn("")->getSkillLevel(), 3);
	EXPECT_THROW(bindXml<model::SkillLearnTemplate>(R"(<skill skillId="21" minLevel="1"/>)")->getSkillLevel(), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::skillengine
