// D12-CP4 (owner, 2026-10-13: D12 = port): PvpMapHandler (Java custom/pvpmap) on an instance of ItemPacketTest's test map, created with the
// handler through WorldMap2DInstance's supplier (onInstanceCreate does not run: its spawns and the random boss's cron job need the PvP map's
// data, which the gate's run with gameserver.pvpmap.enable=true covers once PvpMapService.init is no partial any more). The texts are the
// Java literals.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/custom/pvpmap/PvpMapHandler.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_TELEPORT.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using configs::main::CustomConfig;
using custom::pvpmap::PvpMapHandler;
using serverpackets::SM_BIND_POINT_TELEPORT;

class PvpMapHandlerTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		savedEnabled = CustomConfig::PVP_MAP_ENABLED.exchange(false);
		savedPveAp = CustomConfig::PVP_MAP_PVE_AP_MULTIPLIER.exchange(1.5f);
		pvpInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [this](world::WorldMapInstance& created) {
			handler = PvpMapHandler::create(created);
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(handler);
		});
		clearSent();
	}

	void TearDown() override {
		if (pvpInstance) { // SetUp completed
			player().getController().cancelAllTasks();
			pvpInstance->detachInstanceHandler();
		}
		CustomConfig::PVP_MAP_ENABLED.store(savedEnabled);
		CustomConfig::PVP_MAP_PVE_AP_MULTIPLIER.store(savedPveAp);
		handler = nullptr;
		pvpInstance = nullptr;
		ItemPacketTest::TearDown();
	}

	/** whether a sent packet holds the text as UTF-16LE */
	bool sentText(std::string_view text) {
		std::vector<uint8_t> needle;
		for (char c : text) {
			needle.push_back(static_cast<uint8_t>(c));
			needle.push_back(0);
		}
		for (const std::vector<uint8_t>& bytes : sent())
			if (std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end())
				return true;
		return false;
	}

	bool wasSent(const std::vector<uint8_t>& packet) {
		for (const std::vector<uint8_t>& bytes : sent())
			if (bytes == packet)
				return true;
		return false;
	}

	runtime::DeterministicExecutor& executor() { return dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend()); }

	bool savedEnabled = false;
	float savedPveAp = 0;
	runtime::Ref<world::WorldMapInstance> pvpInstance;
	runtime::Ref<PvpMapHandler> handler;
};

/** PvpMapHandler.java:233-258: a player who is no staff member is told why he cannot join - the map disabled, then his level */
TEST_F(PvpMapHandlerTest, JoinRefusals) {
	handler->join(player());
	EXPECT_TRUE(sentText("The PvP-Map is currently disabled."));
	clearSent();
	CustomConfig::PVP_MAP_ENABLED.store(true);
	handler->join(player());
	EXPECT_TRUE(sentText("The PvP-Map is for players level 60 and above."));
	EXPECT_FALSE(player().getController().hasScheduledTask(model::TaskId::SKILL_USE)) << "no teleportation";
}

/** PvpMapHandler.java:190-221: leaving starts the 10 s teleportation (SM_BIND_POINT_TELEPORT 1, the task, then 3); a second leave while it
 * runs is refused */
TEST_F(PvpMapHandlerTest, LeaveStartsTheTeleportation) {
	handler->leave(player());
	EXPECT_TRUE(wasSent(serializedFor(SM_BIND_POINT_TELEPORT(1, player().getObjectId(), 1, 0))));
	EXPECT_TRUE(player().getController().hasScheduledTask(model::TaskId::SKILL_USE));
	clearSent();
	handler->leave(player());
	EXPECT_TRUE(sentText("You cannot leave the PvP-Map in your current state.")) << "a teleportation is scheduled";
	clearSent();
	executor().advance(std::chrono::milliseconds(10000));
	EXPECT_TRUE(wasSent(serializedFor(SM_BIND_POINT_TELEPORT(3, player().getObjectId(), 1, 0))));
}

/** PvpMapHandler.java:290-300, 419-427, 457-464, 854-857: a death on the disabled map does nothing; no participants; no random boss; the
 * PvE AP multiplier of the config */
TEST_F(PvpMapHandlerTest, TheQueries) {
	EXPECT_TRUE(handler->onDie(player(), player()));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(handler->getParticipantsSize(), 0);
	EXPECT_TRUE(handler->isRandomBoss(0));
	EXPECT_FALSE(handler->isRandomBoss(5));
	EXPECT_FALSE(handler->isRandomBossAlive());
	EXPECT_EQ(handler->getApMultiplier(), 1.5f);
	EXPECT_FALSE(handler->isOnMap(player()));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
