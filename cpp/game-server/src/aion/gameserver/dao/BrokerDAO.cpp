#include "aion/gameserver/dao/BrokerDAO.h"

#include <string>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/broker/BrokerRace.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"

namespace aion::gameserver::dao {

using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using model::gameobjects::BrokerItem;
using model::gameobjects::Item;
using model::gameobjects::Persistable;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.BrokerDAO");

std::vector<runtime::Ref<model::gameobjects::BrokerItem>> BrokerDAO::loadBroker() {
	std::vector<runtime::Ref<BrokerItem>> brokerItems;
	std::vector<runtime::Ref<Item>> items = InventoryDAO::loadBrokerItems();
	ItemStoneListDAO::load(std::vector<runtime::Ptr<Item>>(items.begin(), items.end()));

	DB::select("SELECT * FROM broker", [&](ResultSet& rset) {
		while (rset.next()) {
			int32_t itemPointer = rset.getInt("item_pointer");
			int32_t itemId = rset.getInt("item_id");
			int64_t itemCount = rset.getLong("item_count");
			std::string itemCreator = rset.getString("item_creator");
			int32_t sellerId = rset.getInt("seller_id");
			int64_t price = rset.getLong("price");
			model::broker::BrokerRace itemBrokerRace =
				detail::enumValueOf<model::broker::BrokerRace>(rset.getString("broker_race"), "com.aionemu.gameserver.model.broker.BrokerRace");
			std::optional<commons::database::Timestamp> expireTime = rset.getTimestamp("expire_time");
			std::optional<commons::database::Timestamp> settleTime = rset.getTimestamp("settle_time");
			bool isSold = rset.getBoolean("is_sold");
			bool isSettled = rset.getBoolean("is_settled");
			bool splittingAvailable = rset.getBoolean("splitting_available");

			runtime::Ptr<Item> item;
			if (!isSold)
				for (const runtime::Ref<Item>& brItem : items) {
					if (itemPointer == brItem->getObjectId()) {
						item = brItem;
						break;
					}
				}
			brokerItems.push_back(BrokerItem::create(item, itemId, itemPointer, itemCount, itemCreator, price, sellerId, itemBrokerRace, isSold,
				isSettled, expireTime, settleTime, splittingAvailable));
		}
	});
	return brokerItems;
}

bool BrokerDAO::store(runtime::Ptr<model::gameobjects::BrokerItem> item) {
	bool result = false;

	if (!item) {
		log.warn("Null broker item on save");
		return result;
	}

	switch (item->getPersistentState()) {
		case Persistable::PersistentState::NEW:
			result = insertBrokerItem(*item);
			if (item->getItem())
				InventoryDAO::store(*item->getItem(), item->getSellerId());
			break;

		case Persistable::PersistentState::DELETED:
			result = deleteBrokerItem(*item);
			break;

		case Persistable::PersistentState::UPDATE_REQUIRED:
			result = updateBrokerItem(*item);
			break;

		default:
			break;
	}

	if (result)
		item->setPersistentState(Persistable::PersistentState::UPDATED);

	return result;
}

bool BrokerDAO::insertBrokerItem(model::gameobjects::BrokerItem& item) {
	bool result = DB::insertUpdate(
		"INSERT INTO `broker` (`item_pointer`, `item_id`, `item_count`, `item_creator`, `price`, `broker_race`, `expire_time`, `seller_id`, `is_sold`, `is_settled`, `splitting_available`) VALUES (?,?,?,?,?,?,?,?,?,?,?)",
		[&](PreparedStatement& stmt) {
			stmt.setInt(1, item.getItemUniqueId());
			stmt.setInt(2, item.getItemId());
			stmt.setLong(3, item.getItemCount());
			stmt.setString(4, item.getItemCreator());
			stmt.setLong(5, item.getPrice());
			stmt.setString(6, detail::enumName(item.getItemBrokerRace()));
			stmt.setTimestamp(7, item.getExpireTime());
			stmt.setInt(8, item.getSellerId());
			stmt.setBoolean(9, item.isSold());
			stmt.setBoolean(10, item.isSettled());
			stmt.setBoolean(11, item.isSplittingAvailable());
			stmt.execute();
		});
	return result;
}

bool BrokerDAO::deleteBrokerItem(model::gameobjects::BrokerItem& item) {
	bool result = DB::insertUpdate("DELETE FROM `broker` WHERE `item_pointer` = ? AND `seller_id` = ? AND `expire_time` = ?", [&](PreparedStatement& stmt) {
		stmt.setInt(1, item.getItemUniqueId());
		stmt.setInt(2, item.getSellerId());
		stmt.setTimestamp(3, item.getExpireTime());
		stmt.execute();
	});
	return result;
}

bool BrokerDAO::updateBrokerItem(model::gameobjects::BrokerItem& item) {
	bool result = DB::insertUpdate(
		"UPDATE broker SET `is_sold` = ?, `is_settled` = ?, `settle_time` = ?, `item_count` = ? WHERE `item_pointer` = ? AND `expire_time` = ? AND `seller_id` = ? AND `is_settled` = 0",
		[&](PreparedStatement& stmt) {
			stmt.setBoolean(1, item.isSold());
			stmt.setBoolean(2, item.isSettled());
			stmt.setTimestamp(3, item.getSettleTime());
			stmt.setLong(4, item.getItemCount());
			stmt.setInt(5, item.getItemUniqueId());
			stmt.setTimestamp(6, item.getExpireTime());
			stmt.setInt(7, item.getSellerId());
			stmt.execute();
		});
	return result;
}

} // namespace aion::gameserver::dao
