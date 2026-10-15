// P4-11b movement controller state: the CreatureMoveController fields, NpcMoveController's started/destination state machine (moveToTargetObject,
// moveToPoint, forcedMoveToPoint, moveToNextPoint, resetMove), target coordinates before and after the start, point reach checks, back steps
// and the MovementMask/GlideFlag constants. Expectations are derived by hand from CreatureMoveController.java, NpcMoveController.java,
// MovementMask.java and GlideFlag.java. The Npc controller records onStartMove/onStopMove (ControllersTestSupport.h).

#include "ControllersTestSupport.h"

#include <atomic>
#include <thread>
#include <vector>

#include "aion/gameserver/controllers/movement/GlideFlag.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::controllers::testing {
namespace {

using movement::GlideFlag;
using movement::MovementMask;
using movement::NpcMoveController;
using runtime::Ptr;
using runtime::Ref;

TEST(MovementConstantsTest, JavaByteValues) {
	EXPECT_EQ(MovementMask::IMMEDIATE, 0);
	EXPECT_EQ(MovementMask::GLIDE, 4);
	EXPECT_EQ(MovementMask::FALL, 8);
	EXPECT_EQ(MovementMask::VEHICLE, 16);
	EXPECT_EQ(MovementMask::ABSOLUTE, 32);
	EXPECT_EQ(MovementMask::MANUAL, 64);
	EXPECT_EQ(MovementMask::POSITION, -128) << "(byte) 0x80";
	EXPECT_EQ(MovementMask::NPC_WALK_SLOW, -22) << "(byte) 0xEA";
	EXPECT_EQ(MovementMask::NPC_WALK_FAST, -24);
	EXPECT_EQ(MovementMask::NPC_RUN_SLOW, -28);
	EXPECT_EQ(MovementMask::NPC_RUN_FAST, -30);
	EXPECT_EQ(MovementMask::NPC_STARTMOVE, -32);
	EXPECT_EQ(static_cast<int8_t>(MovementMask::NPC_WALK_SLOW | MovementMask::GLIDE), -18) << "NpcMoveController.getMoveMask of a flying npc: 0xEE";
	EXPECT_EQ(static_cast<int8_t>(MovementMask::NPC_RUN_FAST | MovementMask::GLIDE), -26);
	EXPECT_EQ(GlideFlag::STRONG_UPWIND, 0x30);
	EXPECT_EQ(GlideFlag::GEYSER, -128);
}

class MovementControllerTest : public ControllersTest {};

TEST_F(MovementControllerTest, DirectionAndLastMoveUpdate) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createNpc();
	npc->getPosition()->setXYZH(5.0f, 6.0f, 7.0f, int8_t{12});
	Ptr<NpcMoveController> move = npc->getMoveController();
	ASSERT_TRUE(move);
	EXPECT_FALSE(move->isInMove());
	EXPECT_FALSE(move->isJumping());
	EXPECT_EQ(move->getMovementMask(), MovementMask::IMMEDIATE);

	move->setNewDirection(100.0f, 200.0f, 300.0f, int8_t{42});
	EXPECT_FLOAT_EQ(move->getTargetX2(), 5.0f) << "NpcMoveController: the owner's position while not started";
	EXPECT_FLOAT_EQ(move->getTargetZ2(), 7.0f);

	move->setIsJumping(true);
	move->setInMove(true);
	EXPECT_TRUE(move->isJumping());
	EXPECT_TRUE(move->isInMove());

	int64_t before = move->getLastMoveUpdate();
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	move->updateLastMove();
	EXPECT_GT(move->getLastMoveUpdate(), before);
}

TEST_F(MovementControllerTest, NpcMoveStateMachine) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createNpc();
	RecordingNpcController& controller = npc->recordingController();
	Ptr<NpcMoveController> move = npc->getMoveController();

	EXPECT_TRUE(move->moveToPoint(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(controller.startMoves, 1);
	EXPECT_FLOAT_EQ(move->getTargetX2(), 0.0f) << "started: targetDestX, still 0 before moveToDestination";

	EXPECT_TRUE(move->moveToPoint(4.0f, 5.0f, 6.0f)) << "already moving to a point: only the point changes";
	EXPECT_EQ(controller.startMoves, 1);
	move->moveToTargetObject();
	move->forcedMoveToPoint(7.0f, 8.0f, 9.0f);
	move->moveToNextPoint();
	EXPECT_EQ(controller.startMoves, 1) << "the other starts do nothing while started";

	npc->getPosition()->setXYZH(4.0f, 5.0f, 6.0f, std::nullopt);
	EXPECT_TRUE(move->isReachedPoint()) << "the last moveToPoint point is the owner position";

	move->resetMove();
	EXPECT_EQ(controller.stopMoves, 1);
	EXPECT_FALSE(move->isReachedPoint()) << "the point is reset to (0, 0, 0)";
	EXPECT_FLOAT_EQ(move->getTargetX2(), 4.0f) << "not started any more";

	move->forcedMoveToPoint(7.0f, 8.0f, 9.0f);
	EXPECT_EQ(controller.startMoves, 2);
	EXPECT_FALSE(move->moveToPoint(1.0f, 1.0f, 1.0f)) << "a forced point cannot be replaced by moveToPoint";
	move->resetMove();

	move->moveToTargetObject();
	EXPECT_EQ(controller.startMoves, 3);
	EXPECT_FALSE(move->moveToPoint(1.0f, 1.0f, 1.0f)) << "moving to the target object";
	move->resetMove();
	EXPECT_EQ(controller.stopMoves, 3);

	move->clearBackSteps();
	EXPECT_EQ(move->getMovementMask(), MovementMask::IMMEDIATE);
	EXPECT_THROW(move->setWalkerTemplate(nullptr, 0), runtime::NullPointerException) << "Java: walkerTemplate.getRouteStep on null";
	EXPECT_EQ(move->getWalkerTemplate(), nullptr);
}

TEST_F(MovementControllerTest, ConcurrentStartsNotifyTheControllerOnce) {
	Ref<ControllersTestNpc> npc;
	{
		CONTROLLERS_TEST_SCOPE;
		npc = createNpc();
	}
	constexpr int threads = 8;
	for (int round = 0; round < 50; ++round) {
		std::atomic<bool> go{false};
		std::atomic<int32_t> accepted{0};
		std::vector<std::thread> workers;
		for (int t = 0; t < threads; ++t) {
			workers.emplace_back([&, t] {
				while (!go.load())
					std::this_thread::yield();
				CONTROLLERS_TEST_SCOPE;
				Ptr<NpcMoveController> move = npc->getMoveController();
				if (t % 2 == 0) {
					if (move->moveToPoint(static_cast<float>(t), 0.0f, 0.0f))
						accepted.fetch_add(1);
				} else {
					move->forcedMoveToPoint(static_cast<float>(t), 0.0f, 0.0f);
				}
			});
		}
		go = true;
		for (std::thread& worker : workers)
			worker.join();
		CONTROLLERS_TEST_SCOPE;
		EXPECT_EQ(npc->recordingController().startMoves, round + 1) << "the started compare-and-set lets exactly one caller start";
		npc->getMoveController()->resetMove();
	}
}

} // namespace
} // namespace aion::gameserver::controllers::testing
