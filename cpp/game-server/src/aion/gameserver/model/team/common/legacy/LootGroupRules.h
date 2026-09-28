#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::team::common::legacy {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `TemporaryPlayerTeam.lootGroupRules`), created with create();
 * both constructors only store values (ported). setPlayersInRoll reads the players during a scheduled task: the players are passed as a vector
 * of Refs the task keeps (§7.1, stored by the callee). `misc` is effectively final in fieldmap but assigned by the long constructor only.
 *
 * @author ATracer, xTz
 */
class LootGroupRules : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const LootRuleType lootRule;
	const int32_t misc;
	const int32_t commonItemAbove;
	const int32_t superiorItemAbove;
	const int32_t heroicItemAbove;
	const int32_t fabledItemAbove;
	const int32_t eternalItemAbove;
	const int32_t mythicItemAbove;
	runtime::Field<int32_t> nrMisc{};
	runtime::Field<int32_t> nrRoundRobin{};
	runtime::ConcurrentLinkedDeque<runtime::Ref<drop::DropItem>> itemsToBeDistributed{AION_LOCK_CLASS(LootGroupRules::itemsToBeDistributed)};

protected:
	LootGroupRules();

	LootGroupRules(LootRuleType lootRule, int32_t misc, int32_t commonItemAbove, int32_t superiorItemAbove, int32_t heroicItemAbove,
		int32_t fabledItemAbove, int32_t eternalItemAbove, int32_t mythicItemAbove);

	~LootGroupRules() override;

public:
	/** Java: new LootGroupRules() */
	static runtime::Ref<LootGroupRules> create();

	/** Java: new LootGroupRules(lootRule, misc, commonItemAbove, superiorItemAbove, heroicItemAbove, fabledItemAbove, eternalItemAbove, ...) */
	static runtime::Ref<LootGroupRules> create(LootRuleType lootRule, int32_t misc, int32_t commonItemAbove, int32_t superiorItemAbove,
		int32_t heroicItemAbove, int32_t fabledItemAbove, int32_t eternalItemAbove, int32_t mythicItemAbove);

	bool getQualityRule(templates::item::ItemQuality quality);

	bool isMisc(templates::item::ItemQuality quality);

	LootRuleType getLootRule() const { return lootRule; }

	int32_t getAutodistributionId();

	int32_t getCommonItemAbove() const { return commonItemAbove; }

	int32_t getSuperiorItemAbove() const { return superiorItemAbove; }

	int32_t getHeroicItemAbove() const { return heroicItemAbove; }

	int32_t getFabledItemAbove() const { return fabledItemAbove; }

	int32_t getEternalItemAbove() const { return eternalItemAbove; }

	int32_t getMythicItemAbove() const { return mythicItemAbove; }

	int32_t getNrMisc() const { return nrMisc.get(); }

	void setNrMisc(int32_t value) { nrMisc.set(value); }

	void setPlayersInRoll(std::vector<runtime::Ref<gameobjects::player::Player>> players, int32_t time, int32_t index, int32_t npcId);

	int32_t getNrRoundRobin() const { return nrRoundRobin.get(); }

	void setNrRoundRobin(int32_t value) { nrRoundRobin.set(value); }

	int32_t getMisc() const { return misc; }

	void addItemToBeDistributed(drop::DropItem& dropItem);

	bool containDropItem(drop::DropItem& dropItem);

	void removeItemToBeDistributed(drop::DropItem& dropItem);

	runtime::ConcurrentLinkedDeque<runtime::Ref<drop::DropItem>>& getItemsToBeDistributed() { return itemsToBeDistributed; }
};

} // namespace aion::gameserver::model::team::common::legacy
