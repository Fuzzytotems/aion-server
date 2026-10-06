#pragma once

// Shared fixture of the M5g party tests (m5g-plan.md GR-05, C-09): up to six real Players in one Poeta map instance, each on its own recording
// AionConnection (tests/cm_ak/InWorldPacketRunSupport.h, included by relative path as tests/cm_lz does), so a test drives the group services and
// events in process and reads what every member was sent.
//
// - The group registry of PlayerGroupService is static and the service starts its offline checker on the first group of the process: each
//   test disbands what it formed (TearDown runs PlayerGroupService::disband on any group a member still holds) and ctest runs every case in a
//   process of its own.
// - Expected packets are the server's own serialization of the packet Java constructs at that statement, for the recipient's connection (the
//   group packets are PER_RECIPIENT); their bytes are the gate's business (m5g-plan.md H-02 decodes them from the Java writeImpl).

#include "../../cm_ak/ItemPacketTestSupport.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {

using model::gameobjects::player::Player;
using model::team::group::PlayerGroup;
using model::team::group::PlayerGroupService;
using serverpackets::SM_SYSTEM_MESSAGE;

/** Sets an atomic configuration value for the scope and restores the previous one (the test process loads no properties) */
template <class T>
class ConfigScope {
public:
	ConfigScope(std::atomic<T>& config, T value) : config_(config), previous_(config.load()) { config.store(value); }
	~ConfigScope() { config_.store(previous_); }
	ConfigScope(const ConfigScope&) = delete;
	ConfigScope& operator=(const ConfigScope&) = delete;

private:
	std::atomic<T>& config_;
	const T previous_;
};

/** One party member: the player and his recording connection */
struct Member {
	PlayerFixture f;
	std::unique_ptr<TestClient> client;

	Player& player() const { return *f.player; }
	std::vector<SerializedBody> sent() const { return (*client)->sent(); }
	void clearSent() const { (*client)->clearSent(); }

	/** How many packets of the opcode this member was sent */
	int32_t count(int32_t opcode) const {
		int32_t n = 0;
		for (const SerializedBody& body : sent())
			n += body.opCode == opcode ? 1 : 0;
		return n;
	}

	/** How many packets this member was sent whose bytes equal the packet serialized for his connection */
	int32_t count(AionServerPacket&& packet) const {
		const std::vector<uint8_t> expected = serialized(std::move(packet), client->con());
		int32_t n = 0;
		for (const SerializedBody& body : sent())
			n += *body.bytes == expected ? 1 : 0;
		return n;
	}
};

class TeamTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 23);
		executor = backend.get(); // owned by ThreadPoolManager until the base TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		items::publishPoetaWorldDataOnce();
		// the kinah and item rows of the item packet tests (a kinah split sends inventory updates whose info blob reads the cleanup data)
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, items::ITEM_TEMPLATES_XML));
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(210010000));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
	}

	void TearDown() override {
		for (Member& m : members) {
			if (runtime::Ptr<PlayerGroup> group = m.f.player->getPlayerGroup())
				PlayerGroupService::disband(*group);
		}
		for (Member& m : members) {
			m.f.player->setTarget(nullptr);
			m.f.player->setClientConnection(nullptr);
			m.client.reset();
			m.f = {};
		}
		members.clear();
		storedItems.clear();
		mapInstance = nullptr;
		map = nullptr;
		InWorldPacketTest::TearDown();
		dataholders::DataManager::ITEM_SET_DATA.resetForTests();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	/** Kinah in the member's cube, loaded the way the inventory DAO does (onLoadHandler: no packet) */
	void giveKinah(Member& m, int32_t itemObjId, int64_t count) {
		runtime::Ref<model::gameobjects::Item> item =
			items::loadedItem(itemObjId, items::KINAH, count, model::items::storage::StorageType::CUBE);
		m.player().getStorage(model::items::storage::getId(model::items::storage::StorageType::CUBE))->onLoadHandler(*item);
		storedItems.push_back(item);
	}

	/** A spawned, online member at (x, 100, 50) of the map instance; object ids 720001.. */
	Member& addMember(std::string_view name, float x = 100.0f) {
		Member& m = members.emplace_back();
		const auto index = static_cast<int32_t>(members.size());
		m.f = makePlayer(720000 + index, 9900 + index, name);
		m.f.player->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		m.f.player->getPosition()->setIsSpawned(true);
		m.client = std::make_unique<TestClient>();
		m.client->enterWorld(m.f);
		m.clearSent();
		return m;
	}

	/** A group of the given members, the first as leader, formed as PlayerGroupInvite.acceptRequest does (createGroup, then addPlayer) */
	PlayerGroup& form(std::initializer_list<Member*> list) {
		auto it = list.begin();
		Member& leader = **it++;
		Member& second = **it++;
		PlayerGroupService::createGroup(leader.player(), second.player(), model::team::TeamType::GROUP, 0);
		PlayerGroup& group = *leader.player().getPlayerGroup();
		for (; it != list.end(); ++it)
			PlayerGroupService::addPlayer(group, (*it)->player());
		for (Member& m : members)
			m.clearSent();
		return group;
	}

	void clearAll() {
		for (Member& m : members)
			m.clearSent();
	}

	static constexpr int32_t SM_GROUP_INFO_OPCODE = opcodeOf<serverpackets::SM_GROUP_INFO>;
	static constexpr int32_t SM_GROUP_MEMBER_INFO_OPCODE = opcodeOf<serverpackets::SM_GROUP_MEMBER_INFO>;
	static constexpr int32_t SM_LEAVE_GROUP_MEMBER_OPCODE = opcodeOf<serverpackets::SM_LEAVE_GROUP_MEMBER>;
	static constexpr int32_t SM_QUESTION_WINDOW_OPCODE = opcodeOf<serverpackets::SM_QUESTION_WINDOW>;
	static constexpr int32_t SM_SHOW_BRAND_OPCODE = opcodeOf<serverpackets::SM_SHOW_BRAND>;

	runtime::DeterministicExecutor* executor = nullptr;
	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	std::deque<Member> members;
	std::vector<runtime::Ref<model::gameobjects::Item>> storedItems;
};

} // namespace aion::gameserver::network::aion::clientpackets::testing::team
