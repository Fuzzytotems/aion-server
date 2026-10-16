// Hub headers of the S0b objects group (docs/design/hub-headers.md): the declarations the rest of the port compiles against (class shapes,
// construction paths) and the parts that are ported in S0b (trivial accessors, PlayerCommonData, SpawnGroup/SpawnTemplate constructors and the
// std::optional mapping of Java null strings and enums).

#include <gtest/gtest.h>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/storage/IStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/L10n.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::model::gameobjects {
namespace {

using player::Player;
using player::PlayerCommonData;
using templates::spawns::SpawnGroup;
using templates::spawns::SpawnTemplate;

// HandlerRegistry.h forward-declares model::gameobjects::Creature and requires a non-template class; visible objects are built only through
// VisibleObject::create<T> (the constructors take the protected CreateKey passkey).
static_assert(std::is_class_v<Creature> && std::is_abstract_v<Creature>);
static_assert(std::derived_from<Npc, Creature> && std::derived_from<Summon, Creature> && std::derived_from<Player, Creature>);
static_assert(std::derived_from<house::House, VisibleObject> && std::derived_from<house::House, Persistable>);
static_assert(!std::is_constructible_v<Npc, std::unique_ptr<controllers::NpcController>, templates::spawns::SpawnTemplate&,
	const templates::npc::NpcTemplate*>);
// Item implements the Ref-held interfaces Expirable and StatOwner and forwards their reference counts to RefCounted.
static_assert(std::derived_from<Item, Expirable> && std::derived_from<Item, stats::calc::StatOwner> && std::derived_from<Item, templates::L10n>);
static_assert(runtime::Retainable<Item>);
// Storage is an abstract part that implements IStorage; TemporaryPlayerTeam is the erased (non-template) Java generic.
static_assert(std::derived_from<items::storage::Storage, runtime::OwnedPart> && std::is_abstract_v<items::storage::Storage>);
static_assert(std::is_abstract_v<team::TemporaryPlayerTeam> && std::derived_from<team::TemporaryPlayerTeam, team::GeneralTeam>);

// PlayerCommonData is RefCounted although it derives StaticTemplate through CreatureTemplate: its pointer is no immortal template capture.
static_assert(!runtime::IsStaticTemplate<PlayerCommonData>::value && !runtime::TaskArg<const PlayerCommonData*>);
static_assert(runtime::TaskArg<runtime::Ref<PlayerCommonData>>);

TEST(PlayerCommonDataTest, JavaFieldInitializersAndTrivialAccessors) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// released when the test ends; the next test drains the Reclaimer
	runtime::Ref<PlayerCommonData> pcd = PlayerCommonData::create(1234);
	EXPECT_EQ(pcd->getPlayerObjId(), 1234);
	EXPECT_EQ(pcd->getLevel(), 0);
	EXPECT_EQ(pcd->getTitleId(), -1);
	EXPECT_EQ(pcd->getBonusTitleId(), -1);
	EXPECT_EQ(pcd->getLastOnline(), std::nullopt); // Java null: never online
	EXPECT_EQ(pcd->getBoundRadius(), nullptr);
	pcd->setName("Luno");
	pcd->setMapId(210010000);
	pcd->setHeading(int8_t{60});
	pcd->setLastOnline(commons::database::Timestamp(std::chrono::milliseconds(1000)));
	EXPECT_EQ(pcd->getName(), "Luno");
	EXPECT_EQ(pcd->getMapId(), 210010000);
	EXPECT_EQ(pcd->getHeading(), 60);
	ASSERT_TRUE(pcd->getLastOnline().has_value());
	EXPECT_EQ(pcd->getLastOnline()->time_since_epoch(), std::chrono::milliseconds(1000));
}

TEST(SpawnTemplateTest, TemplatesArePartsOfTheirGroup) {
	static_assert(std::is_base_of_v<runtime::OwnedPart, SpawnTemplate>, "SpawnTemplate is an OwnedPart of SpawnGroup (design §9)");
	static_assert(std::is_same_v<decltype(std::declval<SpawnTemplate&>().getGroup()), SpawnGroup&>);
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		runtime::Ref<SpawnTemplate> spawn;
		{
			runtime::Ref<SpawnGroup> group = SpawnGroup::create(300110000, 214567, 60, nullptr);
			// Java SpawnEngine.newSpawn: new SpawnTemplate(new SpawnGroup(...), x, y, z, heading, 0, null, 0, creatorId, aiName) adds itself
			spawn = SpawnTemplate::create(*group, 1.5f, 2.5f, 3.5f, int8_t{30}, 0, std::nullopt, 0, 11, "dummy");
			EXPECT_EQ(group->getSpawnTemplates().size(), 1);
			EXPECT_EQ(group->getSpawnTemplates().get(0).get(), spawn.get());
			EXPECT_EQ(group->refCount(), 2u) << "a Ref to the template retains its group";
		}
		EXPECT_EQ(spawn->getGroup().refCount(), 1u);
		EXPECT_EQ(spawn->getWorldId(), 300110000); // ported: Npc/Summon constructors read it
		EXPECT_EQ(spawn->getCreatorId(), 11);
		EXPECT_EQ(spawn->getAiName(), std::optional<std::string>("dummy"));
		EXPECT_EQ(spawn->partRefCount(), 1u);

		SpawnGroup& group = spawn->getGroup();
		SpawnTemplate& detached =
			group.adoptDetachedTemplate(std::make_unique<SpawnTemplate>(group, 0.0f, 0.0f, 0.0f, int8_t{0}, 0, std::nullopt, 0));
		EXPECT_EQ(&detached.getGroup(), &group);
		EXPECT_EQ(group.getSpawnTemplates().size(), 1) << "detached templates are not spawn spots";
		spawn.reset(); // releases the group, which destroys both templates
	}
	runtime::Reclaimer::getInstance().drain();
}

/** A spawn template part of its group (subclasses construct it and hand it to the group, like SpawnTemplate::create) */
class TestSpawnTemplate final : public SpawnTemplate {
public:
	TestSpawnTemplate(SpawnGroup& group, std::optional<std::string_view> walkerId, std::optional<std::string_view> aiName)
		: SpawnTemplate(group, 1.5f, 2.5f, 3.5f, int8_t{30}, 5, walkerId, 77, 9, aiName) {}

	static runtime::Ref<TestSpawnTemplate> create(SpawnGroup& group, std::optional<std::string_view> walkerId,
		std::optional<std::string_view> aiName) {
		return runtime::Ref<TestSpawnTemplate>(
			static_cast<TestSpawnTemplate&>(group.addSpawnTemplate(std::make_unique<TestSpawnTemplate>(group, walkerId, aiName))));
	}
};

TEST(SpawnTemplateTest, ConstructorsStoreMembersAndNullStringsAreOptional) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		runtime::Ref<SpawnGroup> group = SpawnGroup::create(300110000, 214567, 60, nullptr);
		EXPECT_EQ(group->getWorldId(), 300110000);
		EXPECT_EQ(group->getNpcId(), 214567);
		EXPECT_EQ(group->getRespawnTime(), 60);
		EXPECT_EQ(group->getPool(), 0);
		EXPECT_EQ(group->getHandlerType(), std::nullopt); // Java null
		EXPECT_EQ(group->getTemporarySpawn(), nullptr);
		EXPECT_TRUE(group->getSpawnTemplates().isEmpty());

		runtime::Ref<TestSpawnTemplate> walker = TestSpawnTemplate::create(*group, "walker_1", std::nullopt);
		EXPECT_EQ(walker->getX(), 1.5f);
		EXPECT_EQ(walker->getHeading(), 30);
		EXPECT_EQ(walker->getRandomWalkRange(), 5);
		EXPECT_EQ(walker->getStaticId(), 77);
		EXPECT_EQ(walker->getCreatorId(), 9);
		EXPECT_EQ(&walker->getGroup(), group.get());
		EXPECT_EQ(walker->getWalkerId(), std::optional<std::string>("walker_1"));
		EXPECT_EQ(walker->getAiName(), std::nullopt);
		walker->setWalkerId(std::nullopt); // handlers: getSpawnTemplate().setWalkerId(null)
		EXPECT_EQ(walker->getWalkerId(), std::nullopt);

		runtime::Ref<TestSpawnTemplate> noAi = TestSpawnTemplate::create(*group, std::nullopt, SpawnTemplate::NO_AI);
		EXPECT_EQ(noAi->getAiName(), std::optional<std::string>(std::string(SpawnTemplate::NO_AI)));
	}
	runtime::Reclaimer::getInstance().drain();
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
