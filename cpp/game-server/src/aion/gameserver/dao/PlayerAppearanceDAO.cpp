#include "aion/gameserver/dao/PlayerAppearanceDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at PlayerAppearanceDAO.java:117 (com.aionemu.gameserver.dao.PlayerAppearanceDAO$1); argument 2 of insertUpdate(); storage: sync

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerAppearanceDAO");

runtime::Ref<model::gameobjects::player::PlayerAppearance> PlayerAppearanceDAO::load(int32_t playerId) {
	AION_UNPORTED();
}

bool PlayerAppearanceDAO::store(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerAppearanceDAO::store(int32_t id, model::gameobjects::player::PlayerAppearance& pa) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
