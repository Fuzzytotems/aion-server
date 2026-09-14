#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"

namespace aion::gameserver::model::team {

/**
 * Hub header (docs/design/hub-headers.md). Java `TemporaryPlayerTeam<TM extends TeamMember<Player>>` is one non-template class (§8.1): TM is
 * spelled TeamMember and GeneralTeam's M is AionObject, so the sendPacket predicate takes `AionObject&` (the Java bridge method); PlayerGroup and
 * PlayerAlliance cast the members to Player. The loot group rules are created by the constructor (Java field initializer).
 *
 * @author ATracer
 */
class TemporaryPlayerTeam : public GeneralTeam {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<common::legacy::LootGroupRules>> lootGroupRules; // Java: = new LootGroupRules() (constructor)

protected:
	runtime::ConcurrentHashMap<int32_t, int32_t> targetIdsByBrandId{};

	TemporaryPlayerTeam(int32_t objId, bool autoReleaseObjectId);
	~TemporaryPlayerTeam() override;

public:
	/** Level of the player with lowest exp */
	virtual int32_t getMinExpPlayerLevel() = 0;

	/** Level of the player with highest exp */
	virtual int32_t getMaxExpPlayerLevel() = 0;

	void updateBrand(int32_t brandId, int32_t targetObjectId);

	void sendBrands(gameobjects::player::Player& member);

	Race getRace() override;

	void sendPackets(std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) override;

	/** Java Predicate<Player> (bridge: the erased GeneralTeam signature) */
	void sendPacket(const std::function<bool(gameobjects::AionObject&)>& predicate,
		std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) override;

	/** Java final */
	std::vector<runtime::Ptr<gameobjects::player::Player>> getOnlineMembers() override final;

	runtime::Ptr<common::legacy::LootGroupRules> getLootGroupRules() override { return lootGroupRules.get(); }

	void setLootGroupRules(runtime::Ptr<common::legacy::LootGroupRules> lootGroupRules);
};

} // namespace aion::gameserver::model::team
