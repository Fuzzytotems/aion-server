#include "aion/gameserver/dao/TownDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `towns` WHERE `race` = ?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `towns`(`id`,`level`,`points`, `race`) VALUES (?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `towns` SET `level` = ?, `points` = ?, `level_up_date` = ? WHERE `id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.TownDAO");

std::unordered_map<int32_t, runtime::Ref<model::town::Town>> TownDAO::load(model::Race race) {
	AION_UNPORTED();
}

void TownDAO::store(model::town::Town& town) {
	AION_UNPORTED();
}

void TownDAO::insertTown(model::town::Town& town) {
	AION_UNPORTED();
}

void TownDAO::updateTown(model::town::Town& town) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
