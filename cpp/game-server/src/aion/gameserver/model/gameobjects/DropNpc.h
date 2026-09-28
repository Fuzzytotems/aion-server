#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Simple
 */
class DropNpc : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t objectIdId;
	runtime::Field<runtime::Ref<runtime::RcHashSet<int32_t>>> allowedLooters{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<player::Player>>>> inRangePlayers{};
	runtime::ArrayList<runtime::Ref<player::Player>> playerStatus{AION_LOCK_CLASS(DropNpc::playerStatus)}; // Java: = new ArrayList<>()
	runtime::Field<runtime::Ref<player::Player>> lootingPlayer{};
	runtime::Field<int32_t> distributionId{0};
	runtime::Field<bool> distributionType{};
	runtime::Field<int32_t> currentIndex{0};
	// fieldmap.toml: java.lang.ref.WeakReference has no counterpart; the drop retains the team until it decays (design §5.1 weak refs: Ref)
	runtime::Field<runtime::Ref<team::TemporaryPlayerTeam>> lootingTeam{};
	runtime::Field<int32_t> lootingTeamId{};
	runtime::Field<int32_t> maxRoll{};
	runtime::Field<runtime::Ref<team::common::legacy::LootGroupRules>> lastLootGroupRules{};
	runtime::Field<bool> isFreeForAll_{false};
	runtime::Field<int64_t> remaingDecayTime{};

protected:
	explicit DropNpc(int32_t objectIdId);

public:
	static runtime::Ref<DropNpc> create(int32_t value);

	/** @param allowedLooters the set to keep (Java stores the caller's set) */
	void setAllowedLooters(runtime::Ptr<runtime::RcHashSet<int32_t>> allowedLooters);

	void setAllowedLooter(player::Player& player);

	runtime::Ptr<runtime::RcHashSet<int32_t>> getAllowedLooters() const { return allowedLooters.get(); }

	bool isAllowedToLoot(player::Player& player);

	void setLootingPlayer(runtime::Ptr<player::Player> player);

	runtime::Ptr<player::Player> getLootingPlayer() const { return this->lootingPlayer.get(); }

	bool isBeingLooted();

	void setDistributionId(int32_t value) { this->distributionId.set(value); }

	int32_t getDistributionId() const { return this->distributionId.get(); }

	void setDistributionType(bool value) { this->distributionType.set(value); }

	bool getDistributionType() const { return this->distributionType.get(); }

	void setCurrentIndex(int32_t value) { this->currentIndex.set(value); }

	int32_t getCurrentIndex() const { return this->currentIndex.get(); }

	int32_t getLootingTeamId() const { return this->lootingTeamId.get(); }

	int32_t getMaxRoll() const { return this->maxRoll.get(); }

	runtime::Ptr<team::common::legacy::LootGroupRules> getLootGroupRules();

	void setLootingTeam(team::TemporaryPlayerTeam& team);

	/** @param inRangePlayers the collection to keep (Java stores the caller's collection) */
	void setInRangePlayers(runtime::Ptr<runtime::RcArrayList<runtime::Ref<player::Player>>> inRangePlayers);

	runtime::Ptr<runtime::RcArrayList<runtime::Ref<player::Player>>> getInRangePlayers() const { return inRangePlayers.get(); }

	void addPlayerStatus(player::Player& player);

	void delPlayerStatus(player::Player& player);

	runtime::ArrayList<runtime::Ref<player::Player>>& getPlayerStatus() { return this->playerStatus; }

	bool containsPlayerStatus(player::Player& player);

	bool isFreeForAll() const { return this->isFreeForAll_.get(); }

	void startFreeForAll();

	int32_t getObjectId() const { return this->objectIdId; }

	int64_t getRemaingDecayTime() const { return this->remaingDecayTime.get(); }

	void setRemaingDecayTime(int64_t value) { this->remaingDecayTime.set(value); }

protected:
	~DropNpc() override;
};

} // namespace aion::gameserver::model::gameobjects
