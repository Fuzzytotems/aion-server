#pragma once

// The fixture of lane A's A1 AI tests (quests, classNpc, walkers, events): PortalsA1AiTest's - the player of
// tests/cm_ak/ItemPacketTestSupport.h moved into a Poeta map instance of its own (a real AionConnection whose send queue the cases read),
// npcs placed beside him from synthetic rows (npc_templates.xml's shape), each AI installed directly - with the rows given by the suite.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::network::aion::clientpackets::testing::items::a1 {

/** One synthetic row per npc id (a dialog npc of the GENERAL tribe; `rating` NORMAL) */
inline std::string a1NpcTemplatesXml(std::initializer_list<int32_t> ids) {
	std::string xml = "<npc_templates>";
	for (int32_t id : ids)
		xml += R"(<npc_template npc_id=")" + std::to_string(id) +
			R"(" level="30" name="a1 npc" name_id="1" height="1" rank="SEASONED" rating="NORMAL" race="NONE" tribe="GENERAL" type="GENERAL">)"
			R"(<stats maxHp="1000"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2"/></stats>)"
			R"(<bound_radius front="0.5" side="0.5" upper="1"/><talk_info distance="10" is_dialog="true"/></npc_template>)";
	return xml + "</npc_templates>";
}

class A1SpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	A1SpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class A1NpcAiTest : public ItemPacketTest {
protected:
	/** The rows of the suite (a1NpcTemplatesXml) */
	virtual std::string rowsXml() = 0;

	void SetUp() override {
		// the world holders are published once per process (tests/ai/AiWorldTestSupport.h), first: the suites' order stays irrelevant
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		// Player::postConstruct loads the toy pets from the database; the tests have none
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		holder = xml::bindString<dataholders::NpcData>(npcContext, rowsXml());
		// the AIs' own spawns read DataManager's npc rows: the same rows
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(publishedContext, rowsXml()));
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		useInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		player().setPosition(
			world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
		clearSent();
	}

	void TearDown() override {
		for (runtime::Ref<model::gameobjects::Npc>& npc : npcs) // the ones no case deleted
			world::World::getInstance().removeObject(*npc);
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		useInstance->detachInstanceHandler();
		useInstance = nullptr;
		ItemPacketTest::TearDown();
		dataholders::DataManager::NPC_DATA.resetForTests();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** An npc of the row, spawned `x` on the x axis (2 m east of the player by default) */
	model::gameobjects::Npc& npcAt(int32_t npcId, float x = 102.0f) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<A1SpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, holder->getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		world::World::getInstance().storeObject(*npc); // VisibleObjectController.delete_ removes it from World
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	template <class AI>
	AI& install(model::gameobjects::Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	std::vector<uint8_t> serializedFor(AionServerPacket&& packet) { return serialized(std::move(packet), client->con()); }

	xml::LoadContext npcContext;
	xml::LoadContext publishedContext;
	std::unique_ptr<dataholders::NpcData> holder;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	runtime::Ref<world::WorldMapInstance> useInstance;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<model::gameobjects::Npc>> npcs;
};

} // namespace aion::gameserver::network::aion::clientpackets::testing::items::a1
