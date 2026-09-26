#include "aion/gameserver/dao/ItemCooldownsDAO.h"

#include <string_view>
#include <tuple>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemCooldown.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::SQLException;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `item_cooldowns` (`player_id`, `delay_id`, `use_delay`, `reuse_time`) VALUES (?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `item_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `delay_id`, `use_delay`, `reuse_time` FROM `item_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ItemCooldownsDAO");

void ItemCooldownsDAO::loadItemCooldowns(model::gameobjects::player::Player& player) {
	DB::select(
		SELECT_QUERY, [&](PreparedStatement& stmt) { stmt.setInt(1, player.getObjectId()); },
		[&](ResultSet& rset) {
			while (rset.next()) {
				int32_t delayId = rset.getInt("delay_id");
				int32_t useDelay = rset.getInt("use_delay");
				int64_t reuseTime = rset.getLong("reuse_time");
				if (reuseTime > commons::utils::currentTimeMillis())
					player.addItemCoolDown(delayId, reuseTime, useDelay);
			}
		});
	player.getEffectController()->broadCastEffects(nullptr);
}

void ItemCooldownsDAO::storeItemCooldowns(model::gameobjects::player::Player& player) {
	deleteItemCooldowns(player);
	auto& itemCoolDowns = player.getItemCoolDowns();
	if (itemCoolDowns.isEmpty())
		return;

	// Java: itemCoolDowns = new HashMap<>(itemCoolDowns); values().removeIf(itemCooldown -> itemCooldown.getReuseTime() - now <= 30000)
	std::vector<std::tuple<int32_t, int32_t, int64_t>> cooldowns;
	for (const auto& entry : itemCoolDowns.snapshot()) {
		const auto& itemCooldown = entry.getValue();
		if (!itemCooldown || itemCooldown->getReuseTime() - commons::utils::currentTimeMillis() <= 30000)
			continue;
		cooldowns.emplace_back(entry.getKey(), itemCooldown->getUseDelay(), itemCooldown->getReuseTime());
	}
	if (cooldowns.empty())
		return;

	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement(INSERT_QUERY);
		con->setAutoCommit(false);
		for (const auto& [delayId, useDelay, reuseTime] : cooldowns) {
			st->setInt(1, player.getObjectId());
			st->setInt(2, delayId);
			st->setInt(3, useDelay);
			st->setLong(4, reuseTime);
			st->addBatch();
		}
		st->executeBatch();
		con->commit();
	} catch (const SQLException& e) {
		log.error("Error while storing item cooldowns for " + player.toString(), e);
	}
}

void ItemCooldownsDAO::deleteItemCooldowns(model::gameobjects::player::Player& player) {
	DB::insertUpdate(DELETE_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, player.getObjectId());
		stmt.execute();
	});
}

} // namespace aion::gameserver::dao
