#include "aion/gameserver/dao/PlayerCooldownsDAO.h"

#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::SQLException;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_cooldowns` (`player_id`, `cooldown_id`, `reuse_delay`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `cooldown_id`, `reuse_delay` FROM `player_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerCooldownsDAO");

void PlayerCooldownsDAO::loadPlayerCooldowns(model::gameobjects::player::Player& player) {
	DB::select(
		SELECT_QUERY, [&](PreparedStatement& stmt) { stmt.setInt(1, player.getObjectId()); },
		[&](ResultSet& rset) {
			while (rset.next()) {
				int32_t cooldownId = rset.getInt("cooldown_id");
				int64_t reuseDelay = rset.getLong("reuse_delay");
				if (reuseDelay > commons::utils::currentTimeMillis())
					player.setSkillCoolDown(cooldownId, reuseDelay);
			}
		});
}

void PlayerCooldownsDAO::storePlayerCooldowns(model::gameobjects::player::Player& player) {
	deletePlayerCooldowns(player);
	auto skillCoolDowns = player.getSkillCoolDowns();
	if (!skillCoolDowns || skillCoolDowns->isEmpty())
		return;

	// Java: cooldowns = new HashMap<>(cooldowns); cooldowns.values().removeIf(reuseTime -> reuseTime - System.currentTimeMillis() <= 28000)
	std::vector<std::pair<int32_t, int64_t>> cooldowns;
	for (const auto& entry : skillCoolDowns->snapshot()) {
		int64_t reuseTime = entry.getValue();
		if (reuseTime - commons::utils::currentTimeMillis() <= 28000)
			continue;
		cooldowns.emplace_back(entry.getKey(), reuseTime);
	}
	if (cooldowns.empty())
		return;

	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement(INSERT_QUERY);
		con->setAutoCommit(false);
		for (const auto& [cooldownId, reuseTime] : cooldowns) {
			st->setInt(1, player.getObjectId());
			st->setInt(2, cooldownId);
			st->setLong(3, reuseTime);
			st->addBatch();
		}
		st->executeBatch();
		con->commit();
	} catch (const SQLException& e) {
		log.error("Couldn't save cooldowns for " + player.toString(), e);
	}
}

void PlayerCooldownsDAO::deletePlayerCooldowns(model::gameobjects::player::Player& player) {
	DB::insertUpdate(DELETE_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, player.getObjectId());
		stmt.execute();
	});
}

} // namespace aion::gameserver::dao
