#include "aion/gameserver/services/drop/DropService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::drop {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.drop.DropService");

DropService::DropService() = default;

DropService::~DropService() = default;

DropService& DropService::getInstance() {
	static DropService instance; // Java SingletonHolder
	return instance;
}

// Defined here (hub-headers.md §9.3): only DropService bodies use it.
class DropService::TempTradeDropPredicate final : public item::ItemService::ItemUpdatePredicate {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<model::gameobjects::DropNpc> dropNpc;
protected:
	explicit TempTradeDropPredicate(model::gameobjects::DropNpc& dropNpc);
public:
	static runtime::Ref<DropService::TempTradeDropPredicate> create(model::gameobjects::DropNpc& value);
	bool changeItem(model::gameobjects::Item& input) override;
protected:
	~TempTradeDropPredicate() override;
};

DropService::TempTradeDropPredicate::TempTradeDropPredicate(model::gameobjects::DropNpc& value) : dropNpc(value) {
}

runtime::Ref<DropService::TempTradeDropPredicate> DropService::TempTradeDropPredicate::create(model::gameobjects::DropNpc& value) {
	return runtime::makeRef<DropService::TempTradeDropPredicate>(value);
}

bool DropService::TempTradeDropPredicate::changeItem(model::gameobjects::Item& input) {
	AION_UNPORTED();
}

DropService::TempTradeDropPredicate::~TempTradeDropPredicate() = default;

// callback at DropService.java:55 (fieldmap key DropService@L55:44)
void DropService::scheduleFreeForAll(int32_t npcUniqueId) {
	AION_UNPORTED();
}

void DropService::unregisterDrop(model::gameobjects::Npc& npc) {
	int32_t npcObjId = npc.getObjectId();
	DropRegistrationService::getInstance().getCurrentDropMap().remove(npcObjId);
	DropRegistrationService::getInstance().getDropRegistrationMap().remove(npcObjId);
}

void DropService::requestDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId) {
	AION_UNPORTED();
}

void DropService::closeDropList(model::gameobjects::player::Player& player, int32_t npcObjectId) {
	using model::gameobjects::state::CreatureState;
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcObjectId);

	player.unsetState(CreatureState::LOOTING);
	player.setState(CreatureState::ACTIVE);
	player.setLootingNpcOid(0);
	utils::PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_EMOTION(player, model::EmotionType::END_LOOT, 0, npcObjectId), true);

	if (!dropNpc)
		return;

	runtime::Ptr<model::gameobjects::player::Player> lootingPlayer = dropNpc->getLootingPlayer();
	if (!lootingPlayer || !player.equals(*lootingPlayer))
		return; // cheater :)

	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> dropItems = DropRegistrationService::getInstance().getCurrentDropMap().get(npcObjectId);
	dropNpc->setLootingPlayer(nullptr);

	runtime::Ptr<model::gameobjects::Npc> npc = runtime::cast<model::gameobjects::Npc>(world::World::getInstance().findVisibleObject(npcObjectId));
	if (npc) {
		if (!dropItems || dropItems->isEmpty()) {
			npc->getController().delete_();
			return;
		}

		RespawnService::scheduleDecayTask(*npc, dropNpc->getRemaingDecayTime());

		runtime::Ptr<model::team::common::legacy::LootGroupRules> lootGroupRules = dropNpc->getLootGroupRules();
		if (lootGroupRules && dropNpc->getInRangePlayers()->size() > 1 && dropNpc->getAllowedLooters()->size() == 1) {
			model::team::common::legacy::LootRuleType lrt = lootGroupRules->getLootRule();
			if (lrt != model::team::common::legacy::LootRuleType::FREEFORALL) {
				for (const runtime::Ptr<model::gameobjects::player::Player>& member : dropNpc->getInRangePlayers()->snapshot()) {
					if (member)
						dropNpc->setAllowedLooter(*member);
				}
				for (const runtime::Ptr<model::drop::DropItem>& dropItem : dropItems->snapshot()) {
					if (!dropItem->getDropTemplate()->isEachMember())
						dropItem->getPlayerObjIds().clear();
				}
			}
		}
		runtime::Ref<model::gameobjects::DropNpc> dropNpcRef(dropNpc);
		utils::PacketSendUtility::broadcastPacket(*npc,
			network::aion::serverpackets::SM_LOOT_STATUS(npcObjectId, network::aion::serverpackets::SM_LOOT_STATUS::Status::LOOT_ENABLE),
			[&dropNpcRef](model::gameobjects::player::Player& receiver) { return dropNpcRef->isAllowedToLoot(receiver); });
	}
}

bool DropService::canDistribute(model::gameobjects::player::Player& player, model::drop::DropItem& requestedItem) {
	AION_UNPORTED();
}

bool DropService::canAutoLoot(model::gameobjects::player::Player& player, model::drop::DropItem& requestedItem) {
	AION_UNPORTED();
}

void DropService::requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex) {
	AION_UNPORTED();
}

void DropService::requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex, bool autoLoot) {
	AION_UNPORTED();
}

void DropService::distributeEqually(model::drop::DropItem& item, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	AION_UNPORTED();
}

void DropService::resendDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId, model::gameobjects::DropNpc& dropNpc, const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& dropItems) {
	AION_UNPORTED();
}

void DropService::winningRollActions(model::gameobjects::player::Player& player, int32_t itemId, int32_t npcObjectId) {
	AION_UNPORTED();
}

void DropService::winningBidActions(model::gameobjects::player::Player& player, int32_t npcObjectId, int64_t highestValue) {
	AION_UNPORTED();
}

void DropService::winningNormalActions(runtime::Ptr<model::gameobjects::player::Player> player, runtime::Ptr<model::gameobjects::DropNpc> dropNpc, model::drop::DropItem& requestedItem) {
	AION_UNPORTED();
}

void DropService::see(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	if (!npc.isDead())
		return;
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npc.getObjectId());
	if (dropNpc && dropNpc->isAllowedToLoot(player)) {
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_LOOT_STATUS(npc.getObjectId(), network::aion::serverpackets::SM_LOOT_STATUS::Status::LOOT_ENABLE));
	}
}

// callback at DropService.java:502 (fieldmap key DropService@L502:4)
void DropService::announceDrop(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* template_) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::drop
