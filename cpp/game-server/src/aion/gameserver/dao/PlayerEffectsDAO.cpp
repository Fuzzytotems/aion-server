#include "aion/gameserver/dao/PlayerEffectsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at PlayerEffectsDAO.java:39 (com.aionemu.gameserver.dao.PlayerEffectsDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at PlayerEffectsDAO.java:114 (com.aionemu.gameserver.dao.PlayerEffectsDAO$2); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_effects` (`player_id`, `skill_id`, `skill_lvl`, `remaining_time`, `end_time`, `force_type`, `magical_criticals`) VALUES (?,?,?,?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_effects` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `skill_id`, `skill_lvl`, `remaining_time`, `end_time`, `force_type`, `magical_criticals` FROM `player_effects` WHERE `player_id`=?";

} // namespace

namespace {

/** Java: the lambda `effect -> effect.canSaveOnLogout() && effect.getRemainingTimeMillis() > 28000` (PlayerEffectsDAO.java:36) */
struct InsertableEffectsPredicate : runtime::TaskStruct {
	bool operator()(skillengine::model::Effect& effect) const { return effect.canSaveOnLogout() && effect.getRemainingTimeMillis() > 28000; }
};

} // namespace

const runtime::PinnedCallback<bool(skillengine::model::Effect&)> PlayerEffectsDAO::insertableEffectsPredicate{InsertableEffectsPredicate{}};

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerEffectsDAO");

void PlayerEffectsDAO::loadPlayerEffects(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerEffectsDAO::storePlayerEffects(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t PlayerEffectsDAO::encodeMagicalCriticalPositions(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

std::unordered_set<int32_t> PlayerEffectsDAO::decodeMagicalCriticalPositions(int32_t bits) {
	AION_UNPORTED();
}

void PlayerEffectsDAO::deletePlayerEffects(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
