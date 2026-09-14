#include "aion/gameserver/dao/PlayerSettingsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at PlayerSettingsDAO.java:66 (com.aionemu.gameserver.dao.PlayerSettingsDAO$1); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerSettingsDAO.java:79 (com.aionemu.gameserver.dao.PlayerSettingsDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerSettingsDAO.java:92 (com.aionemu.gameserver.dao.PlayerSettingsDAO$3); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerSettingsDAO.java:104 (com.aionemu.gameserver.dao.PlayerSettingsDAO$4); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerSettingsDAO.java:115 (com.aionemu.gameserver.dao.PlayerSettingsDAO$5); argument 2 of insertUpdate(); storage: sync

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerSettingsDAO");

runtime::Ref<model::gameobjects::player::PlayerSettings> PlayerSettingsDAO::loadSettings(int32_t playerId) {
	AION_UNPORTED();
}

void PlayerSettingsDAO::saveSettings(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
