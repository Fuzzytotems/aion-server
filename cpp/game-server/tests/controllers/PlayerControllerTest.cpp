// P4-11b PlayerController.onDialogSelect with a client-chosen quest id (CM_DIALOG_SELECT forwards an unchecked questId when the target is another
// player): Java throws NullPointerException on `DataManager.QUEST_DATA.getQuestById(questId).isCannotShare()` for an unknown id, which the packet
// processor logs; the port must throw the same exception instead of dereferencing a null template. Also the PlayableMoveController movement
// direction buckets (PlayableMoveController.setNewDirection), with angles computed by hand from PositionUtil.convertHeadingToAngle (heading * 3)
// and calculateAngleFrom (atan2 in degrees, normalized to [0, 360)).
//
// Test doubles: players are the real Player on stat container doubles (TestPlayer, the PlayerGameStats constructor is P5-01), as in the DAO and
// player tests; QUEST_DATA is a holder bound from XML text.

#include "ControllersTestSupport.h"

#include <memory>
#include <string_view>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::controllers::testing {
namespace {

using movement::PlayableMoveController;
using runtime::Ref;
using Direction = PlayableMoveController::MovementModifierDirection;

class TestPlayerGameStats final : public model::stats::container::CreatureGameStats {
public:
	explicit TestPlayerGameStats(model::gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const model::templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<model::stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

class TestPlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override = default;

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			// PlayerGameStats(Player&) is P5-01: everything before it ran
		}
		setGameStats(std::make_unique<TestPlayerGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

struct PlayerFixture {
	Ref<model::account::Account> account;
	Ref<model::gameobjects::player::PlayerCommonData> commonData;
	Ref<TestPlayer> player;
};

/** Java PlayerService.getPlayer: account, common data, appearance, account data and the account warehouse, then create<Player> */
PlayerFixture makePlayer(int32_t objectId, int32_t accountId, std::string_view name) {
	PlayerFixture f;
	f.account = model::account::Account::create(accountId);
	f.commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
	f.commonData->setName(name);
	f.commonData->setRace(model::Race::ELYOS);
	Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
	f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
	f.account->setAccountWarehouse(
		std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
	f.player = model::gameobjects::VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(objectId), *f.account);
	return f;
}

class PlayerControllerTest : public ControllersTest {
protected:
	void SetUp() override {
		ControllersTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::QUEST_DATA.publish(
			xml::bindString<dataholders::QuestsData>(context, R"(<quests><quest id="1000" cannot_share="true"/></quests>)"));
	}

	void TearDown() override {
		dataholders::DataManager::QUEST_DATA.resetForTests();
		ControllersTest::TearDown();
	}
};

TEST_F(PlayerControllerTest, DialogSelectQuestAcceptWithAnUnknownQuestIdThrowsNullPointerException) {
	CONTROLLERS_TEST_SCOPE;
	PlayerFixture sharer = makePlayer(100001, 9001, "Sharer");
	PlayerFixture responder = makePlayer(100002, 9002, "Responder");
	sharer.player->setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}));
	responder.player->setPosition(world::WorldPosition::create(210010000, 110.0f, 100.0f, 50.0f, int8_t{0}));

	// the quest template is known and cannot be shared: nothing happens (isCannotShare is read without a null dereference)
	EXPECT_NO_THROW(sharer.player->getController().onDialogSelect(model::DialogAction::QUEST_ACCEPT_1, 0, *responder.player, 1000, 0));
	// Java: DataManager.QUEST_DATA.getQuestById(4242) is null, .isCannotShare() throws NullPointerException
	EXPECT_THROW(sharer.player->getController().onDialogSelect(model::DialogAction::QUEST_ACCEPT_1, 0, *responder.player, 4242, 0),
		runtime::NullPointerException);
	EXPECT_THROW(sharer.player->getController().onDialogSelect(model::DialogAction::QUEST_ACCEPT_SIMPLE, 0, *responder.player, 4242, 0),
		runtime::NullPointerException);
	// out of range (another map) or the player himself: the template is never read
	EXPECT_NO_THROW(sharer.player->getController().onDialogSelect(model::DialogAction::QUEST_ACCEPT_1, 0, *sharer.player, 4242, 0));
	responder.player->setPosition(world::WorldPosition::create(220010000, 110.0f, 100.0f, 50.0f, int8_t{0}));
	EXPECT_NO_THROW(sharer.player->getController().onDialogSelect(model::DialogAction::QUEST_ACCEPT_1, 0, *responder.player, 4242, 0));
}

/** PlayableMoveController is abstract in Java (public constructor): a test subclass on the npc of the controller fixture */
class TestPlayableMoveController final : public PlayableMoveController {
public:
	explicit TestPlayableMoveController(model::gameobjects::Creature& owner) : PlayableMoveController(owner) {}
};

class PlayableMoveDirectionTest : public ControllersTest {};

TEST_F(PlayableMoveDirectionTest, MovementDirectionBuckets) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createNpc();
	npc->getPosition()->setXYZH(0.0f, 0.0f, 0.0f, int8_t{0});
	auto move = std::make_unique<TestPlayableMoveController>(*npc);
	move->setInMove(true); // getMovementDirection: NONE only while not moving and more than 1 s after the last move update

	auto directionTowards = [&](int8_t heading, float x, float y) {
		move->setNewDirection(x, y, 0.0f, heading);
		return move->getMovementDirection();
	};
	// heading 0 = 0 degrees; relative angle = heading angle - angle towards the target, folded into [-180, 180]
	EXPECT_EQ(directionTowards(0, 10.0f, 0.0f), Direction::FORWARD) << "0";
	EXPECT_EQ(directionTowards(0, 10.0f, 10.0f), Direction::FORWARD) << "0 - 45 = -45";
	EXPECT_EQ(directionTowards(0, 0.0f, 10.0f), Direction::SIDEWAYS) << "0 - 90 = -90";
	EXPECT_EQ(directionTowards(0, -10.0f, 0.0f), Direction::BACKWARD) << "0 - 180 = -180 (not below -180, so not folded)";
	EXPECT_EQ(directionTowards(0, 0.0f, -10.0f), Direction::SIDEWAYS) << "0 - 270 = -270, folded to 90";
	// bucket edges 67.5 and 112.5 through the heading (3 degrees per step), target on the positive x axis (0 degrees)
	EXPECT_EQ(directionTowards(22, 10.0f, 0.0f), Direction::FORWARD) << "66";
	EXPECT_EQ(directionTowards(23, 10.0f, 0.0f), Direction::SIDEWAYS) << "69";
	EXPECT_EQ(directionTowards(37, 10.0f, 0.0f), Direction::SIDEWAYS) << "111";
	EXPECT_EQ(directionTowards(38, 10.0f, 0.0f), Direction::BACKWARD) << "114";
	EXPECT_EQ(directionTowards(-22, 10.0f, 0.0f), Direction::FORWARD) << "-66 normalized to 294: 294 - 0 = 294, folded to -66";
	EXPECT_EQ(directionTowards(-23, 10.0f, 0.0f), Direction::SIDEWAYS) << "291 folded to -69";
	EXPECT_EQ(directionTowards(-38, 10.0f, 0.0f), Direction::BACKWARD) << "246 folded to -114";

	move->setInMove(false);
	move.reset();
}

} // namespace
} // namespace aion::gameserver::controllers::testing
