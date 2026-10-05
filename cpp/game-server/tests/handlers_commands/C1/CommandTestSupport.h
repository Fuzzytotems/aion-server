#pragma once

// The fixture of the chat command tests (tests/handlers_commands/C1): a real Player with a real AionConnection (tests/cm_ak/
// InWorldPacketRunSupport.h, by relative path as tests/playersvc includes tests/instance's fixture), the commands.properties access levels of
// the stage-0 aliases, LOG_GMAUDIT on, and the expected bytes of sendInfo / sendMessage.
//
// Expectations are written from the Java: what reaches the client is PacketSendUtility.sendMessage's SM_MESSAGE(0, null, text, GOLDEN_YELLOW)
// per ChatUtil.split part, compared with the server's serialization of that packet (as GMServiceLoginTest does).

#include "../../cm_ak/InWorldPacketRunSupport.h"
#include "../../cm_ak/ItemPacketTestSupport.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {

using model::gameobjects::player::Player;
using serverpackets::SM_MESSAGE;

/** SM_PLAYER_INFO's two reads of services these tests have not got (HousingService loads from the database), as TravelTestSupport.h stubs them */
inline runtime::Ptr<model::house::House> noHouse(Player&) {
	return nullptr;
}

inline runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> noCpInfo(Player&) {
	return nullptr;
}

class CommandTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		items::publishPoetaWorldDataOnce();
		previousLevels = configs::administration::CommandsConfig::ACCESS_LEVELS.get();
		std::map<std::string, int8_t, std::less<>> levels(*previousLevels);
		// commands.properties' levels of the stage-0 admin aliases (3 for the GM tools); the framework's scripted commands 3, 2 and 3
		for (const char* alias : {"invis", "invul", "enemy", "see", "coords", "info", "zone", "online", "time", "weather", "addexp", "set", "addskill",
				 "delskill", "addtitle", "heal", "speed", "dispel", "morph", "state", "stat", "levelup", "leveldown", "removecd", "clearusercoolt", "fwtest", "fwconsole"})
			levels[alias] = 3;
		levels["fwplayer"] = 2;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		previousGmAudit = configs::main::LoggingConfig::LOG_GMAUDIT.load();
		configs::main::LoggingConfig::LOG_GMAUDIT.store(true);
		lookups.activeHouseOfPlayer = &noHouse;
		lookups.cpInfoForCurrentMap = &noCpInfo;
		serverpackets::detail::setPacketLookupsForTests(&lookups);
	}

	void TearDown() override {
		serverpackets::detail::setPacketLookupsForTests(nullptr);
		for (PlayerFixture& fixture : players)
			fixture.player->setClientConnection(nullptr);
		clients.clear();
		players.clear();
		// the map instance the spawned players' positions name, after them and before the base fixture (ItemPacketTestSupport.h's order)
		mapInstance = nullptr;
		map = nullptr;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(*previousLevels);
		configs::main::LoggingConfig::LOG_GMAUDIT.store(previousGmAudit);
		InWorldPacketTest::TearDown();
	}

	/** A connected character of an account with `accessLevel` (and `membership`) */
	Player& connected(int32_t objectId, std::string_view name, int8_t accessLevel, int8_t membership = 0) {
		PlayerFixture& fixture = players.emplace_back(makePlayer(objectId, objectId + 1000, name));
		fixture.account->setAccessLevel(accessLevel);
		fixture.account->setMembership(membership);
		fixture.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*fixture.player)); // SM_PLAYER_INFO
		// the parts PlayerService.loadPlayer gives every character that the commands reach: the skill list (//addskill, //delskill) and the
		// NPC factions (a level change: PlayerController.onLevelChange)
		if (fixture.player->getSkillList() == nullptr)
			fixture.player->setSkillList(model::skill::PlayerSkillList::create());
		fixture.player->setNpcFactions(std::make_unique<model::gameobjects::player::npcFaction::NpcFactions>(*fixture.player));
		TestClient& client = *clients.emplace_back(std::make_unique<TestClient>());
		client.enterWorld(fixture);
		client->clearSent();
		return *fixture.player;
	}

	TestClient& client(size_t index = 0) { return *clients[index]; }

	/**
	 * Puts the player into a Poeta map instance's region at (100, 100, 50) and marks him spawned, as ItemPacketTestSupport.h does: the paths
	 * that read the map region (a level change's PlayerController.updateNearbyQuests) need it.
	 */
	void spawnInPoeta(Player& player) {
		if (mapInstance == nullptr) {
			map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(210010000));
			mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
				return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
					::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
			});
		}
		player.setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player.getPosition()->setIsSpawned(true);
	}

	/** PacketSendUtility.sendMessage(player, text): SM_MESSAGE(0, null, text, GOLDEN_YELLOW) */
	std::vector<uint8_t> message(std::string_view text, size_t index = 0) {
		return serialized(SM_MESSAGE(0, "", text, model::ChatType::GOLDEN_YELLOW), client(index).con());
	}

	/** what sendInfo(player, text) sends: one message per ChatUtil.split part */
	std::vector<std::vector<uint8_t>> info(std::string_view text, size_t index = 0) {
		std::vector<std::vector<uint8_t>> packets;
		for (const std::string& part : utils::ChatUtil::split(text))
			packets.push_back(message(part, index));
		return packets;
	}

	/** the first packet the client was sent (an Enemy arm's message; onChangedPlayerAttributes' packets follow it) */
	std::vector<uint8_t> firstSent() {
		const std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
		return sent.empty() ? std::vector<uint8_t>{} : sent.front();
	}

	static std::vector<std::string> args(std::initializer_list<std::string_view> values) { return {values.begin(), values.end()}; }

	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	std::vector<PlayerFixture> players;
	std::vector<std::unique_ptr<TestClient>> clients;
	std::shared_ptr<const std::map<std::string, int8_t, std::less<>>> previousLevels;
	bool previousGmAudit = false;
	serverpackets::detail::PacketLookupsForTests lookups;
};

} // namespace aion::gameserver::network::aion::clientpackets::testing
