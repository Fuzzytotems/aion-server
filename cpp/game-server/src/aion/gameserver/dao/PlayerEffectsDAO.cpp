#include "aion/gameserver/dao/PlayerEffectsDAO.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::SQLException;
using skillengine::model::Effect;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_effects` (`player_id`, `skill_id`, `skill_lvl`, `remaining_time`, `end_time`, `force_type`, `magical_criticals`) VALUES (?,?,?,?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_effects` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `skill_id`, `skill_lvl`, `remaining_time`, `end_time`, `force_type`, `magical_criticals` FROM `player_effects` WHERE `player_id`=?";

/** Java: the lambda `effect -> effect.canSaveOnLogout() && effect.getRemainingTimeMillis() > 28000` (PlayerEffectsDAO.java:36) */
struct InsertableEffectsPredicate : runtime::TaskStruct {
	bool operator()(skillengine::model::Effect& effect) const { return effect.canSaveOnLogout() && effect.getRemainingTimeMillis() > 28000; }
};

} // namespace

const runtime::PinnedCallback<bool(skillengine::model::Effect&)> PlayerEffectsDAO::insertableEffectsPredicate{InsertableEffectsPredicate{}};

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerEffectsDAO");

void PlayerEffectsDAO::loadPlayerEffects(model::gameobjects::player::Player& player) {
	DB::select(
		SELECT_QUERY, [&](PreparedStatement& stmt) { stmt.setInt(1, player.getObjectId()); },
		[&](ResultSet& rset) {
			while (rset.next()) {
				int32_t skillId = rset.getInt("skill_id");
				int32_t skillLvl = rset.getInt("skill_lvl");
				int32_t remainingTime = rset.getInt("remaining_time");
				int64_t endTime = rset.getLong("end_time");
				std::optional<std::string> forceTypeStr = rset.getObject<std::string>("force_type");
				const skillengine::model::Effect_ForceType* forceType =
					!forceTypeStr ? nullptr : skillengine::model::Effect_ForceType::getInstance(*forceTypeStr);
				std::unordered_set<int32_t> magicalCriticalPositions = decodeMagicalCriticalPositions(rset.getInt("magical_criticals"));
				player.getEffectController()->addSavedEffect(skillId, skillLvl, remainingTime, endTime, forceType, &magicalCriticalPositions);
			}
		});
	player.getEffectController()->broadCastEffects(nullptr);
}

void PlayerEffectsDAO::storePlayerEffects(model::gameobjects::player::Player& player) {
	deletePlayerEffects(player);
	std::vector<runtime::Ptr<Effect>> effects;
	for (const runtime::Ptr<Effect>& effect : player.getEffectController()->getAbnormalEffects()) {
		if (insertableEffectsPredicate(*effect))
			effects.push_back(effect);
	}
	if (effects.empty())
		return;

	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement(INSERT_QUERY);
		con->setAutoCommit(false);
		for (const runtime::Ptr<Effect>& effect : effects) {
			ps->setInt(1, player.getObjectId());
			ps->setInt(2, effect->getSkillId());
			ps->setInt(3, effect->getSkillLevel());
			ps->setInt(4, static_cast<int32_t>(effect->getRemainingTimeMillis()));
			ps->setLong(5, effect->getEndTime());
			ps->setString(6, !effect->getForceType() ? std::optional<std::string>() : std::optional<std::string>(effect->getForceType()->getName()));
			ps->setInt(7, encodeMagicalCriticalPositions(*effect));
			ps->addBatch();
		}
		ps->executeBatch();
		con->commit();
	} catch (const SQLException& e) {
		log.error("Exception while saving effects of player " + std::to_string(player.getObjectId()), e);
	}
}

int32_t PlayerEffectsDAO::encodeMagicalCriticalPositions(skillengine::model::Effect& effect) {
	uint32_t bits = 0;
	for (const skillengine::effect::EffectTemplate* effectTemplate : effect.getEffectTemplates())
		if (effect.isMagicalCritical(effectTemplate->getPosition()))
			bits |= 1u << ((static_cast<uint32_t>(effectTemplate->getPosition()) - 1) & 31); // Java masks the shift count to 5 bits
	return static_cast<int32_t>(bits);
}

std::unordered_set<int32_t> PlayerEffectsDAO::decodeMagicalCriticalPositions(int32_t bits) {
	std::unordered_set<int32_t> positions;
	// Java: for (int position = 1; bits != 0; position++, bits >>= 1) (an arithmetic shift: a negative value never reaches 0; the column is a
	// TINYINT, so the bits stay below 128)
	for (int32_t position = 1; bits != 0; position++, bits >>= 1)
		if ((bits & 1) != 0)
			positions.insert(position);
	return positions;
}

void PlayerEffectsDAO::deletePlayerEffects(model::gameobjects::player::Player& player) {
	DB::insertUpdate(DELETE_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, player.getObjectId());
		stmt.execute();
	});
}

} // namespace aion::gameserver::dao
