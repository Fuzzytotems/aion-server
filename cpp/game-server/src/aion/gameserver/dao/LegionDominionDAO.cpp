#include "aion/gameserver/dao/LegionDominionDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at LegionDominionDAO.java:66 (com.aionemu.gameserver.dao.LegionDominionDAO$1); argument 2 of insertUpdate(); storage: sync
//   anonymous ParamReadStH at LegionDominionDAO.java:82 (com.aionemu.gameserver.dao.LegionDominionDAO$2); argument 2 of select(); storage: sync
//   anonymous IUStH at LegionDominionDAO.java:110 (com.aionemu.gameserver.dao.LegionDominionDAO$3); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at LegionDominionDAO.java:123 (com.aionemu.gameserver.dao.LegionDominionDAO$4); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view UPDATE_LOC = "UPDATE legion_dominion_locations SET legion_id=?, occupied_date=? WHERE id=?";
constexpr std::string_view LOAD1 = "SELECT * FROM `legion_dominion_locations`";
constexpr std::string_view LOAD2 = "SELECT * FROM `legion_dominion_participants` WHERE `legion_dominion_id`=? ";
constexpr std::string_view INSERT_NEW_LOCATION = "INSERT INTO legion_dominion_locations(`id`,`legion_id`) VALUES(?,?)";
constexpr std::string_view INSERT_NEW = "INSERT INTO legion_dominion_participants(`legion_dominion_id`, `legion_id`) VALUES (?, ?)";
constexpr std::string_view UPDATE_PARTICIPANT = "UPDATE legion_dominion_participants SET points=?, survived_time=?, participated_date=? WHERE legion_id=?";
constexpr std::string_view DELETE_INFO = "DELETE FROM legion_dominion_participants WHERE legion_id=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.LegionDominionDAO");

bool LegionDominionDAO::loadOrCreateLegionDominionLocations(const std::unordered_map<int32_t,
	runtime::Ptr<model::legionDominion::LegionDominionLocation>>& locations) {
	AION_UNPORTED();
}

void LegionDominionDAO::updateLegionDominionLocation(model::legionDominion::LegionDominionLocation& loc) {
	AION_UNPORTED();
}

std::map<int32_t, runtime::Ref<model::legionDominion::LegionDominionParticipantInfo>> LegionDominionDAO::loadParticipants(
	model::legionDominion::LegionDominionLocation& loc) {
	AION_UNPORTED();
}

void LegionDominionDAO::storeNewInfo(int32_t id, model::legionDominion::LegionDominionParticipantInfo& info) {
	AION_UNPORTED();
}

void LegionDominionDAO::updateInfo(model::legionDominion::LegionDominionParticipantInfo& info) {
	AION_UNPORTED();
}

void LegionDominionDAO::delete_(model::legionDominion::LegionDominionParticipantInfo& info) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
