#include "aion/gameserver/dao/PlayerPasskeyDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_passkey` (`account_id`, `passkey`) VALUES (?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_passkey` SET `passkey`=? WHERE `account_id`=? AND `passkey`=?";
constexpr std::string_view UPDATE_FORCE_QUERY = "UPDATE `player_passkey` SET `passkey`=? WHERE `account_id`=?";
constexpr std::string_view CHECK_QUERY = "SELECT COUNT(*) cnt FROM `player_passkey` WHERE `account_id`=? AND `passkey`=?";
constexpr std::string_view EXIST_CHECK_QUERY = "SELECT COUNT(*) cnt FROM `player_passkey` WHERE `account_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerPasskeyDAO");

void PlayerPasskeyDAO::insertPlayerPasskey(int32_t accountId, std::string_view passkey) {
	AION_UNPORTED();
}

bool PlayerPasskeyDAO::updatePlayerPasskey(int32_t accountId, std::string_view oldPasskey, std::string_view newPasskey) {
	AION_UNPORTED();
}

bool PlayerPasskeyDAO::updateForcePlayerPasskey(int32_t accountId, std::string_view newPasskey) {
	AION_UNPORTED();
}

bool PlayerPasskeyDAO::checkPlayerPasskey(int32_t accountId, std::string_view passkey) {
	AION_UNPORTED();
}

bool PlayerPasskeyDAO::existCheckPlayerPasskey(int32_t accountId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
