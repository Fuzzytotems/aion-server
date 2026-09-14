#include "aion/gameserver/dao/BonusPackDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `bonus_packs` (`account_id`, `receiving_player`) VALUES (?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `receiving_player` FROM `bonus_packs` WHERE `account_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.BonusPackDAO");

int32_t BonusPackDAO::loadReceivingPlayer(int32_t accountId) {
	AION_UNPORTED();
}

bool BonusPackDAO::storeReceivingPlayer(int32_t accountId, int32_t playerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
