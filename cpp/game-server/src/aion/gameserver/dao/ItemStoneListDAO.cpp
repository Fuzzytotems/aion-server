#include "aion/gameserver/dao/ItemStoneListDAO.h"

#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ItemStone.h"
#include "aion/gameserver/model/items/ItemStone_ItemStoneType.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/HashMap.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::gameobjects::Item;
using model::gameobjects::Persistable;
using model::items::GodStone;
using model::items::IdianStone;
using model::items::ItemStone;
using model::items::ItemStone_ItemStoneType;
using model::items::ManaStone;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `item_stones` (`item_unique_id`, `item_id`, `slot`, `category`, `polishNumber`, `polishCharge`, `proc_count`) VALUES (?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `item_stones` SET `item_id`=?, `slot`=?, `polishNumber`=?, `polishCharge`=?, `proc_count`=? where `item_unique_id`=? AND `category`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `item_stones` WHERE `item_unique_id`=? AND slot=? AND category=?";
constexpr std::string_view SELECT_QUERY = "SELECT `item_id`, `slot`, `category`, `polishNumber`, `polishCharge`, `proc_count` FROM `item_stones` WHERE `item_unique_id`=?";

/** Java DataManager.ITEM_DATA.getItemTemplate(itemId).getItemGroup(): NullPointerException for an unknown item id */
model::templates::item::enums::ItemGroup itemGroupOf(int32_t itemId) {
	const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (!itemTemplate)
		throw runtime::NullPointerException("Cannot invoke \"ItemTemplate.getItemGroup()\" because the item template " + std::to_string(itemId) + " is null");
	return itemTemplate->getItemGroup();
}

int32_t ordinal(ItemStone_ItemStoneType ist) {
	return static_cast<int32_t>(ist);
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ItemStoneListDAO");

void ItemStoneListDAO::load(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	std::vector<runtime::Ptr<Item>> validItems;
	for (const runtime::Ptr<Item>& i : items) {
		if (i->getItemTemplate()->isArmor() || i->getItemTemplate()->isWeapon())
			validItems.push_back(i);
	}

	if (validItems.empty())
		return;

	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		for (const runtime::Ptr<Item>& item : validItems) {
			stmt->setInt(1, item->getObjectId());
			auto rset = stmt->executeQuery();
			while (rset->next()) {
				int32_t itemId = rset->getInt("item_id");
				int32_t slot = rset->getInt("slot");
				int32_t stoneType = rset->getInt("category");
				int32_t activatedCount = rset->getInt("proc_count");
				switch (stoneType) {
					case 0:
						if (item->getSockets(false) <= item->getItemStonesSize()) {
							log.warn("Deleting manastone " + std::to_string(itemId) + " due to slot overload from " + item->toString());
							deleteItemStone(*con, item->getObjectId(), slot, stoneType);
							continue;
						}
						if (itemGroupOf(itemId) == model::templates::item::enums::ItemGroup::SPECIAL_MANASTONE &&
							slot >= item->getItemTemplate()->getSpecialSlots()) {
							log.warn("Deleting special manastone " + std::to_string(itemId) + " from normal slot of " + item->toString());
							deleteItemStone(*con, item->getObjectId(), slot, stoneType);
							continue;
						}
						item->getItemStones()->add(ManaStone::create(item->getObjectId(), itemId, slot, Persistable::PersistentState::UPDATED));
						break;
					case 1: {
						item->addGodStone(itemId, activatedCount);
						runtime::Ptr<GodStone> godstone = item->getGodStone();
						if (godstone)
							godstone->setPersistentState(Persistable::PersistentState::UPDATED);
						break;
					}
					case 2:
						if (item->getSockets(true) <= item->getFusionStonesSize()) {
							log.warn("Deleting manastone " + std::to_string(itemId) + " due to slot overload from fusioned item of " + item->toString());
							deleteItemStone(*con, item->getObjectId(), slot, stoneType);
							continue;
						}
						if (itemGroupOf(itemId) == model::templates::item::enums::ItemGroup::SPECIAL_MANASTONE &&
							slot >= item->getFusionedItemTemplate()->getSpecialSlots()) {
							log.warn("Deleting special manastone " + std::to_string(itemId) + " from normal slot of fusioned item of " + item->toString());
							deleteItemStone(*con, item->getObjectId(), slot, stoneType);
							continue;
						}
						item->getFusionStones()->add(ManaStone::create(item->getObjectId(), itemId, slot, Persistable::PersistentState::UPDATED));
						break;
					case 3:
						item->setIdianStone(std::make_unique<IdianStone>(itemId, Persistable::PersistentState::UPDATE_REQUIRED, *item,
							rset->getInt("polishNumber"), rset->getInt("polishCharge")));
						break;
					default:
						break;
				}
			}
		}
	} catch (const std::exception& e) {
		log.error("Could not restore ItemStoneList data from DB: " + std::string(e.what()), e);
	}
}

void ItemStoneListDAO::save(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	if (items.empty()) {
		return;
	}

	std::unordered_set<runtime::Ptr<ItemStone>> manaStones;
	std::unordered_set<runtime::Ptr<ItemStone>> fusionStones;
	std::unordered_set<runtime::Ptr<ItemStone>> godStones;
	std::unordered_set<runtime::Ptr<ItemStone>> idianStones;

	for (const runtime::Ptr<Item>& item : items) {
		if (item->hasManaStones()) {
			for (const runtime::Ptr<ManaStone>& stone : item->getItemStones()->snapshot())
				manaStones.insert(runtime::Ptr<ItemStone>(stone));
		}

		if (item->hasFusionStones()) {
			for (const runtime::Ptr<ManaStone>& stone : item->getFusionStones()->snapshot())
				fusionStones.insert(runtime::Ptr<ItemStone>(stone));
		}

		runtime::Ptr<GodStone> godStone = item->getGodStone();
		if (godStone) {
			godStones.insert(runtime::Ptr<ItemStone>(godStone));
		}

		runtime::Ptr<IdianStone> idianStone = item->getIdianStone();
		if (idianStone) {
			idianStones.insert(runtime::Ptr<ItemStone>(idianStone));
		}
	}

	store(manaStones, ItemStone_ItemStoneType::MANASTONE);
	store(fusionStones, ItemStone_ItemStoneType::FUSIONSTONE);
	store(godStones, ItemStone_ItemStoneType::GODSTONE);
	store(idianStones, ItemStone_ItemStoneType::IDIANSTONE);
}

void ItemStoneListDAO::storeManaStones(const std::unordered_set<runtime::Ptr<model::items::ManaStone>>& manaStones) {
	store(std::unordered_set<runtime::Ptr<ItemStone>>(manaStones.begin(), manaStones.end()), ItemStone_ItemStoneType::MANASTONE);
}

void ItemStoneListDAO::storeGodStones(model::items::GodStone& godStones) {
	store(std::unordered_set<runtime::Ptr<ItemStone>>{runtime::Ptr<ItemStone>(&godStones)}, ItemStone_ItemStoneType::GODSTONE);
}

void ItemStoneListDAO::storeFusionStone(const std::unordered_set<runtime::Ptr<model::items::ManaStone>>& manaStones) {
	store(std::unordered_set<runtime::Ptr<ItemStone>>(manaStones.begin(), manaStones.end()), ItemStone_ItemStoneType::FUSIONSTONE);
}

void ItemStoneListDAO::storeIdianStones(model::items::IdianStone& idianStone) {
	store(std::unordered_set<runtime::Ptr<ItemStone>>{runtime::Ptr<ItemStone>(&idianStone)}, ItemStone_ItemStoneType::IDIANSTONE);
}

void ItemStoneListDAO::store(const std::unordered_set<runtime::Ptr<model::items::ItemStone>>& stones, model::items::ItemStone_ItemStoneType ist) {
	if (stones.empty()) {
		return;
	}

	std::vector<runtime::Ptr<ItemStone>> stonesToAdd;
	std::vector<runtime::Ptr<ItemStone>> stonesToDelete;
	std::vector<runtime::Ptr<ItemStone>> stonesToUpdate;
	for (const runtime::Ptr<ItemStone>& stone : stones) {
		if (stone->getPersistentState() == Persistable::PersistentState::NEW)
			stonesToAdd.push_back(stone);
		else if (stone->getPersistentState() == Persistable::PersistentState::DELETED)
			stonesToDelete.push_back(stone);
		else if (stone->getPersistentState() == Persistable::PersistentState::UPDATE_REQUIRED)
			stonesToUpdate.push_back(stone);
	}

	try {
		auto con = DatabaseFactory::getConnection();
		con->setAutoCommit(false);
		deleteItemStones(*con, stonesToDelete, ist);
		addItemStones(*con, stonesToAdd, ist);
		updateItemStones(*con, stonesToUpdate, ist);
	} catch (const SQLException& e) {
		log.error("Can't save stones", e);
	}

	for (const runtime::Ptr<ItemStone>& is : stones) {
		is->setPersistentState(Persistable::PersistentState::UPDATED);
	}
}

void ItemStoneListDAO::addItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
	model::items::ItemStone_ItemStoneType ist) {
	if (itemStones.empty()) {
		return;
	}

	try {
		auto st = con.prepareStatement(INSERT_QUERY);
		for (const runtime::Ptr<ItemStone>& is : itemStones) {
			st->setInt(1, is->getItemObjId());
			st->setInt(2, is->getItemId());
			st->setInt(3, is->getSlot());
			st->setInt(4, ordinal(ist));
			if (auto* stone = dynamic_cast<IdianStone*>(is.get())) {
				st->setInt(5, stone->getPolishNumber());
				st->setInt(6, stone->getPolishCharge());
			} else {
				st->setInt(5, 0);
				st->setInt(6, 0);
			}
			if (auto* gs = dynamic_cast<GodStone*>(is.get())) {
				st->setInt(7, gs->getActivatedCount());
			} else {
				st->setInt(7, 0);
			}
			st->addBatch();
		}

		st->executeBatch();
		con.commit();
	} catch (const SQLException& e) {
		log.error("Error occured while saving item stones", e);
	}
}

void ItemStoneListDAO::updateItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
	model::items::ItemStone_ItemStoneType ist) {
	if (itemStones.empty()) {
		return;
	}

	try {
		auto st = con.prepareStatement(UPDATE_QUERY);
		for (const runtime::Ptr<ItemStone>& is : itemStones) {
			st->setInt(1, is->getItemId());
			st->setInt(2, is->getSlot());
			if (auto* stone = dynamic_cast<IdianStone*>(is.get())) {
				st->setInt(3, stone->getPolishNumber());
				st->setInt(4, stone->getPolishCharge());
			} else {
				st->setInt(3, 0);
				st->setInt(4, 0);
			}
			if (auto* gs = dynamic_cast<GodStone*>(is.get())) {
				st->setInt(5, gs->getActivatedCount());
			} else {
				st->setInt(5, 0);
			}
			st->setInt(6, is->getItemObjId());
			st->setInt(7, ordinal(ist));
			st->addBatch();
		}

		st->executeBatch();
		con.commit();
	} catch (const SQLException& e) {
		log.error("Error occured while saving item stones", e);
	}
}

void ItemStoneListDAO::deleteItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
	model::items::ItemStone_ItemStoneType ist) {
	if (itemStones.empty()) {
		return;
	}

	try {
		auto st = con.prepareStatement(DELETE_QUERY);
		// TODO: Shouldn't we update stone slot?
		for (const runtime::Ptr<ItemStone>& is : itemStones) {
			st->setInt(1, is->getItemObjId());
			st->setInt(2, is->getSlot());
			st->setInt(3, ordinal(ist));
			st->execute();
			st->addBatch();
		}

		st->executeBatch();
		con.commit();
	} catch (const SQLException& e) {
		log.error("Error occured while saving item stones", e);
	}
}

void ItemStoneListDAO::deleteItemStone(commons::database::Connection& con, int32_t uid, int32_t slot, int32_t category) {
	try {
		auto st = con.prepareStatement(DELETE_QUERY);
		st->setInt(1, uid);
		st->setInt(2, slot);
		st->setInt(3, category);
		st->execute();
	} catch (const SQLException& e) {
		log.error("Error occured while saving item stones", e);
	}
}

void ItemStoneListDAO::save(model::gameobjects::player::Player& player) {
	save(player.getAllItems());
}

} // namespace aion::gameserver::dao
