#include "aion/gameserver/services/drop/DropService.h"

#include <algorithm>
#include <any>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/main/DropConfig.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/InRoll.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemQualityInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_LOOT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_ITEMLIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/toypet/PetService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::drop {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.drop.DropService");

namespace {

/** Java: DataManager.ITEM_DATA.getItemTemplate(itemId) dereferenced right away (NullPointerException for an unknown item) */
const model::templates::item::ItemTemplate* itemTemplateOf(int32_t itemId) {
	const model::templates::item::ItemTemplate* template_ = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (template_ == nullptr)
		throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(itemId) + ")");
	return template_;
}

/** Java: an ItemQuality handed to LootGroupRules, whose switch (getQualityRule) or equals (isMisc) throws NullPointerException for null */
model::templates::item::ItemQuality requireQuality(const std::optional<model::templates::item::ItemQuality>& quality) {
	if (!quality)
		throw runtime::NullPointerException("itemTemplate.getItemQuality()");
	return *quality;
}

/** Java: requestedItem.getWinningPlayer() dereferenced right away (NullPointerException for null) */
model::gameobjects::player::Player& winningPlayerOf(model::drop::DropItem& requestedItem) {
	runtime::Ptr<model::gameobjects::player::Player> winningPlayer = requestedItem.getWinningPlayer();
	if (!winningPlayer)
		throw runtime::NullPointerException("requestedItem.getWinningPlayer()");
	return *winningPlayer;
}

} // namespace

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

// callback at DropService.java:55 (fieldmap key DropService@L55:44): captures only the npc id, so an unpinned task (bindTask)
void DropService::scheduleFreeForAll(int32_t npcUniqueId) {
	utils::ThreadPoolManager::getInstance().schedule(runtime::bindTask(
		[](int32_t npcUniqueId) {
			runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcUniqueId);
			if (dropNpc) {
				// java-race: Java reads the map a second time; an unregisterDrop in between is its NullPointerException
				runtime::Ptr<model::gameobjects::DropNpc> registered = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcUniqueId);
				if (!registered)
					throw runtime::NullPointerException("dropRegistrationMap.get(" + std::to_string(npcUniqueId) + ")");
				registered->startFreeForAll();
				runtime::Ptr<model::gameobjects::VisibleObject> visibleObject = world::World::getInstance().findVisibleObject(npcUniqueId);
				if (visibleObject && visibleObject->isSpawned()) {
					// fix for elyos/asmodians being able to loot elyos/asmodian npcs
					// TODO there might be more npcs who are friendly towards players and should not be loot able by them
					runtime::Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(visibleObject);
					if (npc && model::isAsmoOrEly(npc->getRace())) {
						utils::PacketSendUtility::broadcastPacket(*npc,
							network::aion::serverpackets::SM_LOOT_STATUS(npcUniqueId, network::aion::serverpackets::SM_LOOT_STATUS::Status::LOOT_ENABLE),
							[&npc](model::gameobjects::player::Player& p) { return npc->getRace() != p.getRace(); });
					} else {
						utils::PacketSendUtility::broadcastPacket(*visibleObject,
							network::aion::serverpackets::SM_LOOT_STATUS(npcUniqueId, network::aion::serverpackets::SM_LOOT_STATUS::Status::LOOT_ENABLE));
					}
				}
			}
		},
		npcUniqueId), 240000);
}

void DropService::unregisterDrop(model::gameobjects::Npc& npc) {
	int32_t npcObjId = npc.getObjectId();
	DropRegistrationService::getInstance().getCurrentDropMap().remove(npcObjId);
	DropRegistrationService::getInstance().getDropRegistrationMap().remove(npcObjId);
}

void DropService::requestDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId) {
	using model::gameobjects::state::CreatureState;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcObjectId);
	if (!player || !dropNpc) {
		return;
	}

	if (player->isLooting())
		closeDropList(*player, player->getLootingNpcOid());

	if (!dropNpc->isAllowedToLoot(*player)) {
		utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_LOOT_NO_RIGHT());
		return;
	}

	if (dropNpc->isBeingLooted()) {
		// java-race: Java reads the looting player again; a closeDropList in between is its NullPointerException
		runtime::Ptr<model::gameobjects::player::Player> lootingPlayer = dropNpc->getLootingPlayer();
		if (!lootingPlayer)
			throw runtime::NullPointerException("dropNpc.getLootingPlayer()");
		if (!lootingPlayer->isOnline()) {
			runtime::Ptr<model::gameobjects::VisibleObject> corpse = world::World::getInstance().findVisibleObject(npcObjectId);
			log.warn(lootingPlayer->toString() + " is offline but was still set as drop looter for " + (corpse ? corpse->toString() : "null"));
		} else {
			utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_LOOT_FAIL_ONLOOTING());
			return;
		}
	}

	dropNpc->setLootingPlayer(player);
	runtime::Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(world::World::getInstance().findVisibleObject(npcObjectId));
	if (npc) {
		runtime::FutureRef decayTask = npc->getController().cancelTask(model::TaskId::DECAY);
		if (decayTask) {
			int64_t remaingDecayTime = decayTask->getDelay(runtime::TimeUnit::MILLISECONDS);
			dropNpc->setRemaingDecayTime(remaingDecayTime);
		}
	}

	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> dropItems = DropRegistrationService::getInstance().getCurrentDropMap().get(npcObjectId);

	// Java: Collections.emptySet() for a missing set; C++: SM_LOOT_ITEMLIST takes the entries (m5b3-plan.md D10) - java-race: Java's packet
	// iterates the live set without its monitor
	std::unordered_set<runtime::Ptr<model::drop::DropItem>> entries;
	if (dropItems) {
		for (const runtime::Ptr<model::drop::DropItem>& dropItem : dropItems->snapshot())
			entries.insert(dropItem);
	}

	utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_LOOT_ITEMLIST(*dropNpc, entries, *player));
	utils::PacketSendUtility::sendPacket(*player,
		network::aion::serverpackets::SM_LOOT_STATUS(npcObjectId, network::aion::serverpackets::SM_LOOT_STATUS::Status::OPEN_DROP_LIST));
	player->unsetState(CreatureState::ACTIVE);
	player->setState(CreatureState::LOOTING);
	player->setLootingNpcOid(npcObjectId);
	utils::PacketSendUtility::broadcastPacket(*player, network::aion::serverpackets::SM_EMOTION(*player, model::EmotionType::START_LOOT, 0, npcObjectId), true);
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
	using model::gameobjects::player::Player;
	int32_t npcId = requestedItem.getNpcObj();
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcId);
	if (!dropNpc) {
		return false;
	}

	runtime::Ptr<model::team::common::legacy::LootGroupRules> lootGroupRules = dropNpc->getLootGroupRules();
	if (!lootGroupRules) {
		return true;
	}

	int32_t itemId = requestedItem.getDropTemplate()->getItemId();
	if (itemId != model::items::ItemId::KINAH) {
		if (dropNpc->getInRangePlayers()->size() > 1) {
			std::optional<model::templates::item::ItemQuality> quality = itemTemplateOf(itemId)->getItemQuality();
			dropNpc->setDistributionId(lootGroupRules->getAutodistributionId());
			dropNpc->setDistributionType(lootGroupRules->getQualityRule(requireQuality(quality)));
		} else
			dropNpc->setDistributionId(0);
		if (dropNpc->getDistributionId() > 1 && dropNpc->getDistributionType()) {
			bool containDropItem = lootGroupRules->containDropItem(requestedItem);
			if (lootGroupRules->getItemsToBeDistributed().isEmpty() || containDropItem) {
				dropNpc->setCurrentIndex(requestedItem.getIndex());
				for (const runtime::Ptr<Player>& member : dropNpc->getInRangePlayers()->snapshot()) {
					runtime::Ptr<Player> finalPlayer = world::World::getInstance().getPlayer(member->getObjectId());
					if (finalPlayer && finalPlayer->isOnline()) {
						dropNpc->addPlayerStatus(*finalPlayer);
						finalPlayer->setPlayerMode(model::actions::PlayerMode::IN_ROLL,
							std::any(model::gameobjects::player::InRoll::create(npcId, itemId, requestedItem.getIndex(), dropNpc->getDistributionId())));
						utils::PacketSendUtility::sendPacket(*finalPlayer,
							network::aion::serverpackets::SM_GROUP_LOOT(dropNpc->getLootingTeamId(), 0, itemId, static_cast<int32_t>(requestedItem.getCount()),
								npcId, dropNpc->getDistributionId(), 1, requestedItem.getIndex()));
					}
				}
				std::vector<runtime::Ref<Player>> inRangePlayers;
				for (const runtime::Ptr<Player>& member : dropNpc->getInRangePlayers()->snapshot())
					inRangePlayers.emplace_back(member);
				lootGroupRules->setPlayersInRoll(std::move(inRangePlayers), dropNpc->getDistributionId() == 2 ? 17000 : 32000, requestedItem.getIndex(),
					npcId);
			} else {
				utils::PacketSendUtility::sendPacket(player,
					network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_LOOT_ALREADY_DISTRIBUTING_ITEM(itemTemplateOf(itemId)->getL10n()));
			}
			if (!containDropItem) {
				lootGroupRules->addItemToBeDistributed(requestedItem);
			}
			return false;
		}
	}
	return true;
}

bool DropService::canAutoLoot(model::gameobjects::player::Player& player, model::drop::DropItem& requestedItem) {
	using model::gameobjects::player::Player;
	int32_t npcId = requestedItem.getNpcObj();
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcId);
	if (!dropNpc) {
		return false;
	}
	runtime::Ptr<model::team::common::legacy::LootGroupRules> lootGroupRules = dropNpc->getLootGroupRules();
	if (!lootGroupRules) {
		return true;
	}

	int32_t itemId = requestedItem.getDropTemplate()->getItemId();
	if (itemId == model::items::ItemId::KINAH)
		return true;

	int32_t distId = lootGroupRules->getAutodistributionId();
	if (dropNpc->getInRangePlayers()->size() <= 1) {
		distId = 0;
		dropNpc->setDistributionId(distId);
	}

	std::optional<model::templates::item::ItemQuality> quality = itemTemplateOf(itemId)->getItemQuality();
	if (distId > 1 && lootGroupRules->getQualityRule(requireQuality(quality))) {
		bool anyOnline = false;
		for (const runtime::Ptr<Player>& member : dropNpc->getInRangePlayers()->snapshot()) {
			runtime::Ptr<Player> finalPlayer = world::World::getInstance().getPlayer(member->getObjectId());
			if (finalPlayer && finalPlayer->isOnline()) {
				anyOnline = true;
				break;
			}
		}
		return !anyOnline;
	}
	return true;
}

void DropService::requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex) {
	requestDropItem(player, npcObjectId, itemIndex, false);
}

void DropService::requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex, bool autoLoot) {
	using model::gameobjects::player::Player;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> dropItems = DropRegistrationService::getInstance().getCurrentDropMap().get(npcObjectId);
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcObjectId);
	runtime::Ref<model::drop::DropItem> requestedItem = nullptr;
	// drop was unregistered
	if (!dropItems || !dropNpc) {
		return;
	}

	SYNCHRONIZED(*dropItems) {
		for (const runtime::Ptr<model::drop::DropItem>& dropItem : dropItems->snapshot())
			if (dropItem->getIndex() == itemIndex) {
				requestedItem = dropItem;
				break;
			}
	}

	if (!requestedItem) // lag can cause drops to be displayed long enough for the client to send multiple loot requests when spamming 'C'
		return;

	// fix exploit
	if (!requestedItem->isDistributeItem() && !dropNpc->isAllowedToLoot(player)) {
		return;
	}

	int32_t itemId = requestedItem->getDropTemplate()->getItemId();
	const model::templates::item::ItemTemplate* template_ = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (template_ == nullptr)
		throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(itemId) + ")");
	if (template_->hasLimitOne()) {
		if (player.getInventory().getFirstItemByItemId(itemId)
			|| player.getStorage(model::items::storage::getId(model::items::storage::StorageType::REGULAR_WAREHOUSE))->getFirstItemByItemId(itemId)) {
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CAN_NOT_GET_LORE_ITEM(template_->getL10n()));
			return;
		}
	}

	runtime::Ptr<model::team::common::legacy::LootGroupRules> lootGroupRules = dropNpc->getLootGroupRules();
	if (lootGroupRules && !requestedItem->isDistributeItem() && !requestedItem->isFreeForAll()) {
		if (lootGroupRules->containDropItem(*requestedItem)) {
			if (!autoLoot)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LOOT_ALREADY_DISTRIBUTING_ITEM(template_->getL10n()));
			return;
		}

		if (autoLoot && !canAutoLoot(player, *requestedItem))
			return;

		requestedItem->setNpcObj(npcObjectId);
		if (!canDistribute(player, *requestedItem)) {
			return;
		}
	}

	int64_t initialCount = requestedItem->getCount();
	// Kinah is distributed to all group/alliance members nearby.
	if (itemId == model::items::ItemId::KINAH) {
		runtime::Ptr<model::team::TemporaryPlayerTeam> team = player.getCurrentTeam();
		if (!team) {
			requestedItem->setCount(item::ItemService::addItem(player, itemId, requestedItem->getCount()));
		} else {
			std::vector<runtime::Ptr<Player>> entitledPlayers;
			for (const runtime::Ptr<model::gameobjects::AionObject>& member : team->filterMembers([&player](model::gameobjects::AionObject& object) {
					 Player& m = static_cast<Player&>(object); // the members of a TemporaryPlayerTeam are Players (TemporaryPlayerTeam.h)
					 return m.isOnline() && !m.isDead() && !m.isMentor()
						 && utils::PositionUtil::isInRange(m, player, static_cast<float>(configs::main::GroupConfig::GROUP_MAX_DISTANCE.load()));
				 }))
				entitledPlayers.push_back(runtime::cast<Player>(member));
			distributeEqually(*requestedItem, entitledPlayers);
		}
	} else if (!player.isInTeam() && !requestedItem->isItemWonNotCollected() && dropNpc->getDistributionId() == 0) {
		requestedItem->setCount(item::ItemService::addItem(player, itemId, requestedItem->getCount()));
	} else if (!requestedItem->isDistributeItem()) {
		if (lootGroupRules) {
			std::optional<model::templates::item::ItemQuality> quality = template_->getItemQuality(); // Java: ITEM_DATA.getItemTemplate(itemId) again
			if (lootGroupRules->isMisc(requireQuality(quality))) {
				runtime::Ptr<runtime::RcArrayList<runtime::Ref<Player>>> members = dropNpc->getInRangePlayers();

				if (members->size() > lootGroupRules->getNrMisc()) {
					lootGroupRules->setNrMisc(lootGroupRules->getNrMisc() + 1);
				} else {
					lootGroupRules->setNrMisc(1);
				}

				int32_t i = 0;
				for (const runtime::Ptr<Player>& p : members->snapshot()) {
					i++;
					if (i == lootGroupRules->getNrMisc()) {
						requestedItem->setWinningPlayer(p);
						break;
					}
				}
			} else {
				requestedItem->setWinningPlayer(player);
			}
		} else if (!requestedItem->getWinningPlayer()) {
			requestedItem->setWinningPlayer(player);
		}

		if (requestedItem->getWinningPlayer()) {
			runtime::Ref<TempTradeDropPredicate> predicate = TempTradeDropPredicate::create(*dropNpc);
			requestedItem->setCount(item::ItemService::addItem(*requestedItem->getWinningPlayer(), itemId, requestedItem->getCount(), false, *predicate));

			winningNormalActions(player, dropNpc, *requestedItem);
		}
	} else if (!autoLoot && requestedItem->isDistributeItem()) { // handles distribution of item to correct player and messages accordingly
		runtime::Ptr<Player> winningPlayer = requestedItem->getWinningPlayer(); // Java: player.equals(null) is false
		if (!(winningPlayer && player.equals(*winningPlayer)) && requestedItem->isItemWonNotCollected()) {
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LOOT_ANOTHER_OWNER_ITEM());
			return;
		} else if (winningPlayerOf(*requestedItem).getInventory().isFull(template_->getExtraInventoryId())) {
			utils::PacketSendUtility::sendPacket(winningPlayerOf(*requestedItem), SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR());
			requestedItem->isItemWonNotCollected(true);
			return;
		}

		runtime::Ref<TempTradeDropPredicate> predicate = TempTradeDropPredicate::create(*dropNpc);
		requestedItem->setCount(item::ItemService::addItem(winningPlayerOf(*requestedItem), itemId, requestedItem->getCount(), false, *predicate));

		switch (dropNpc->getDistributionId()) {
			case 2:
				winningRollActions(winningPlayerOf(*requestedItem), itemId, npcObjectId);
				break;
			case 3:
				winningBidActions(winningPlayerOf(*requestedItem), npcObjectId, requestedItem->getHighestValue());
				break;
		}
	}

	if (requestedItem->getCount() <= 0) {
		SYNCHRONIZED(*dropItems) {
			dropItems->remove(requestedItem);
		}
	}
	if (requestedItem->getCount() < initialCount) {
		announceDrop(requestedItem->getWinningPlayer() ? winningPlayerOf(*requestedItem) : player, template_);
		runtime::Ptr<model::gameobjects::Pet> pet = player.getPet();
		if (pet && pet->getCommonData()->isSelling()) {
			std::vector<runtime::Ptr<model::gameobjects::Item>> stacks = player.getInventory().getItemsByItemId(requestedItem->getDropTemplate()->getItemId());
			if (std::ranges::any_of(stacks, [](const runtime::Ptr<model::gameobjects::Item>& item) {
					std::optional<model::templates::item::ItemQuality> quality = item->getItemTemplate()->getItemQuality();
					return item->isSellable() && quality && *quality == model::templates::item::ItemQuality::JUNK;
				})) {
				toypet::PetService::getInstance().sell(pet, stacks);
			}
		}
	}

	if (!autoLoot) {
		// java-race: Java passes the live set, which SM_LOOT_ITEMLIST streams without its monitor; C++: its entries now (m5b3-plan.md D10)
		std::unordered_set<runtime::Ptr<model::drop::DropItem>> entries;
		for (const runtime::Ptr<model::drop::DropItem>& dropItem : dropItems->snapshot())
			entries.insert(dropItem);
		resendDropList(dropNpc->getLootingPlayer(), npcObjectId, *dropNpc, entries);
	}
}

void DropService::distributeEqually(model::drop::DropItem& item, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	if (players.empty())
		return;
	int64_t countPerPlayer = item.getCount() / static_cast<int64_t>(players.size());
	for (int32_t i = static_cast<int32_t>(players.size()) - 1; i >= 0; i--) {
		int64_t count = i == 0 ? item.getCount() : countPerPlayer;
		int64_t remainingCount = item::ItemService::addItem(*players[static_cast<size_t>(i)], item.getDropTemplate()->getItemId(), count);
		item.setCount(item.getCount() - count + remainingCount);
	}
}

void DropService::resendDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId, model::gameobjects::DropNpc& dropNpc, const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& dropItems) {
	using model::gameobjects::state::CreatureState;
	runtime::Ptr<model::gameobjects::Npc> npc = runtime::cast<model::gameobjects::Npc>(world::World::getInstance().findVisibleObject(npcObjectId));
	if (dropItems.size() != 0) {
		if (player) {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_LOOT_ITEMLIST(dropNpc, dropItems, *player));
		}
	} else {
		if (player) {
			utils::PacketSendUtility::sendPacket(*player,
				network::aion::serverpackets::SM_LOOT_STATUS(npcObjectId, network::aion::serverpackets::SM_LOOT_STATUS::Status::CLOSE_DROP_LIST));
			player->unsetState(CreatureState::LOOTING);
			player->setState(CreatureState::ACTIVE);
			utils::PacketSendUtility::broadcastPacket(*player, network::aion::serverpackets::SM_EMOTION(*player, model::EmotionType::END_LOOT, 0, npcObjectId), true);
		}
		if (npc) {
			npc->getController().delete_();
		}
	}
}

void DropService::winningRollActions(model::gameobjects::player::Player& player, int32_t itemId, int32_t npcObjectId) {
	using model::gameobjects::player::Player;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	std::string itemL10n = itemTemplateOf(itemId)->getL10n();
	utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LOOT_GET_ITEM_ME(itemL10n));

	if (player.isInTeam()) {
		runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcObjectId);
		if (!dropNpc)
			throw runtime::NullPointerException("dropRegistrationMap.get(" + std::to_string(npcObjectId) + ")");
		for (const runtime::Ptr<Player>& member : dropNpc->getInRangePlayers()->snapshot()) {
			if (member && !player.equals(*member)) {
				utils::PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_LOOT_GET_ITEM_OTHER(player.getName(), itemL10n));
			}
		}
	}
}

void DropService::winningBidActions(model::gameobjects::player::Player& player, int32_t npcObjectId, int64_t highestValue) {
	using model::gameobjects::player::Player;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcObjectId);
	if (highestValue > 0) {
		if (!player.getInventory().tryDecreaseKinah(highestValue)) {
			return;
		}
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_PAY_ACCOUNT_ME(highestValue));
	}

	if (!dropNpc)
		throw runtime::NullPointerException("dropRegistrationMap.get(" + std::to_string(npcObjectId) + ")");
	std::vector<runtime::Ptr<Player>> onlineMembers; // Java: stream().filter(p -> p.isOnline() && !p.equals(player)).toList()
	for (const runtime::Ptr<Player>& p : dropNpc->getInRangePlayers()->snapshot())
		if (p->isOnline() && !p->equals(player))
			onlineMembers.push_back(p);
	for (const runtime::Ptr<Player>& member : onlineMembers) {
		utils::PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_PAY_ACCOUNT_OTHER(player.getName(), highestValue));
		int64_t distributeKinah = highestValue / static_cast<int64_t>(onlineMembers.size());
		member->getInventory().increaseKinah(distributeKinah);
		utils::PacketSendUtility::sendPacket(*member,
			SM_SYSTEM_MESSAGE::STR_MSG_PAY_DISTRIBUTE(highestValue, static_cast<int32_t>(onlineMembers.size()), distributeKinah));
	}
}

void DropService::winningNormalActions(runtime::Ptr<model::gameobjects::player::Player> player, runtime::Ptr<model::gameobjects::DropNpc> dropNpc, model::drop::DropItem& requestedItem) {
	using model::gameobjects::player::Player;
	if (!player || !dropNpc)
		return;

	int32_t itemId = requestedItem.getDropTemplate()->getItemId();
	if (player->isInTeam()) {
		for (const runtime::Ptr<Player>& member : dropNpc->getInRangePlayers()->snapshot()) {
			if (member && !winningPlayerOf(requestedItem).equals(*member) && member->isOnline())
				utils::PacketSendUtility::sendPacket(*member, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GET_ITEM_PARTYNOTICE(
																 winningPlayerOf(requestedItem).getName(), itemTemplateOf(itemId)->getL10n()));
		}
	}
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

// callback at DropService.java:502 (fieldmap key DropService@L502:4): the filter captures the player, so it pins it
void DropService::announceDrop(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* template_) {
	using model::gameobjects::player::Player;
	std::optional<configs::detail::ItemQuality> minAnnounceQuality = configs::main::DropConfig::MIN_ANNOUNCE_QUALITY.load();
	if (!minAnnounceQuality || player.isInInstance())
		return;
	std::optional<model::templates::item::ItemQuality> quality = template_->getItemQuality();
	if (!quality)
		throw runtime::NullPointerException("template.getItemQuality()");
	// the config enum has ItemQuality's constants in the same order (configs/detail/ConfigEnums.h)
	if (model::templates::item::getQualityId(*quality) <
		model::templates::item::getQualityId(static_cast<model::templates::item::ItemQuality>(static_cast<int32_t>(*minAnnounceQuality))))
		return;
	utils::PacketSendUtility::broadcastToMap(player,
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_FORCE_ITEM_WIN(player.getName(), utils::ChatUtil::item(template_->getTemplateId())), 0,
		runtime::PinnedCallback<bool(Player&)>(runtime::Pin{&player}, [&player](Player& p) { return !p.equals(player) && p.getRace() == player.getRace(); }));
}

} // namespace aion::gameserver::services::drop
