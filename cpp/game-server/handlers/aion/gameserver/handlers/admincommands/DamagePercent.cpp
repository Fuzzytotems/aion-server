#include "aion/gameserver/handlers/admincommands/DamagePercent.h"

#include <exception>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(DamagePercent);

DamagePercent::DamagePercent() : AdminCommand("damagepercent") {
}

// C++-dev only: //damage's target rules (Damage.java:30-57), then a percentage from 0 to 100, with or without a trailing %
void DamagePercent::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.size() < 1 || params.size() > 2) {
		info(admin, std::nullopt);
		return;
	}

	runtime::Ptr<VisibleObject> target = admin.getTarget();
	if (target == nullptr) {
		PacketSendUtility::sendMessage(admin, "No target selected");
		return;
	}
	runtime::Ptr<Creature> creature = runtime::as<Creature>(target);
	if (creature == nullptr)
		return; // Java's //damage does nothing for a target that is no creature
	std::string damageType = damageTypeOf(params[0]);
	runtime::Ptr<Player> player = runtime::as<Player>(creature);
	if ((damageType == "fp" || damageType == "dp") && player == nullptr) {
		info(admin, std::nullopt);
		return;
	}
	if (damageType != "hp" && params.size() != 2) {
		info(admin, std::nullopt);
		return;
	}

	std::string value = damageType == "hp" ? params[0] : params[1];
	if (value.ends_with('%'))
		value.pop_back();
	int32_t percent = 0;
	try {
		percent = commons::utils::parseInt(value);
	} catch (const std::exception&) {
		info(admin, std::nullopt);
		return;
	}
	if (percent < 0 || percent > 100) {
		info(admin, std::nullopt);
		return;
	}
	apply(*creature, player, damageType, percent);
}

void DamagePercent::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax //damagepercent (mp/fp/dp) <percent | percent%>\n<percent> must be a number from 0 to 100 "
										   "(of the max HP/MP/FP, DP: of 4000).\n(mp/fp/dp) is optional, leave out to use HP damage\nin case of "
										   "fp/dp, target must be player!");
}

std::string DamagePercent::damageTypeOf(const std::string& param) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (equalsIgnoreCase(param, "mp"))
		return "mp";
	if (equalsIgnoreCase(param, "fp"))
		return "fp";
	if (equalsIgnoreCase(param, "dp"))
		return "dp";
	return "hp";
}

// Java Damage.java:83-101 with isPercent: the percentage of the max, then Java's damage path of the type
void DamagePercent::apply(Creature& creature, runtime::Ptr<Player> player, const std::string& damageType, int32_t percent) {
	using TYPE = network::aion::serverpackets::SM_ATTACK_STATUS::TYPE;
	using LOG = network::aion::serverpackets::SM_ATTACK_STATUS::LOG;
	using geoEngine::math::JavaFloat;
	if (damageType == "hp") {
		int32_t dmg = JavaFloat::doubleToInt(static_cast<float>(percent) / 100.0f * static_cast<float>(creature.getLifeStats()->getMaxHp()));
		creature.getController().onAttack(creature, dmg, std::nullopt);
	} else if (damageType == "mp") {
		int32_t dmg = JavaFloat::doubleToInt(static_cast<float>(percent) / 100.0f * static_cast<float>(creature.getLifeStats()->getMaxMp()));
		creature.getLifeStats()->reduceMp(TYPE::DAMAGE_MP, dmg, 0, LOG::MPATTACK);
	} else if (damageType == "fp") {
		int32_t dmg = JavaFloat::doubleToInt(static_cast<float>(percent) / 100.0f * static_cast<float>(player->getLifeStats()->getMaxFp()));
		player->getLifeStats()->reduceFp(TYPE::FP_DAMAGE, dmg, 0, LOG::FPATTACK);
	} else if (damageType == "dp") {
		int32_t dmg = JavaFloat::doubleToInt(static_cast<float>(percent) / 100.0f * 4000.0f);
		if (dmg > player->getCommonData()->getDp())
			dmg = player->getCommonData()->getDp();
		player->getCommonData()->setDp(player->getCommonData()->getDp() - dmg);
	}
}

// the access level of //damage (commands.properties: damage)
std::string DamagePercent::getAliasForLevel() {
	return "damage";
}

} // namespace aion::gameserver::handlers::admincommands
