#include "aion/gameserver/handlers/admincommands/Heal.h"

#include <algorithm>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_EXP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Heal);

Heal::Heal()
	: AdminCommand("heal", "Restores HP, MP, DP, flight time and energy of repose.",
		  " - Heals your target's HP, MP and removes soul sickness.\n"
		  "dp - Heals your target's DP.\n"
		  "fp - Heals your target's flight time.\n"
		  "repose - Heals your target's energy of repose.\n"
		  "<number> - Heals your target's HP by given amount.\n"
		  "<number%> - Heals your target's HP by given percentage.\n") {
}

// Java Heal.java:35-83
void Heal::execute(Player& player, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	using TYPE = network::aion::serverpackets::SM_ATTACK_STATUS::TYPE; // parity: the C++ alias of Java's import of SM_ATTACK_STATUS.TYPE
	using LOG = network::aion::serverpackets::SM_ATTACK_STATUS::LOG; // parity: the C++ alias of Java's import of SM_ATTACK_STATUS.LOG
	runtime::Ptr<VisibleObject> target = player.getTarget();
	if (target == nullptr) {
		sendInfo(player);
		return;
	}
	runtime::Ptr<Creature> creature = runtime::as<Creature>(target);
	if (creature == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}
	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(creature);
	if (params.empty()) {
		creature->getLifeStats()->increaseHp(TYPE::HP, creature->getLifeStats()->getMaxHp());
		creature->getLifeStats()->increaseMp(TYPE::HEAL_MP, creature->getLifeStats()->getMaxMp(), 0, LOG::MPHEAL);
		creature->getEffectController()->removeByDispelSlotType(DispelSlotType::SPECIAL2);
		if (!player.equals(*creature))
			sendInfo(player, name(*creature) + " has been refreshed.");
	} else if (equalsIgnoreCase(params[0], "dp") && targetPlayer != nullptr) {
		targetPlayer->getCommonData()->setDp(targetPlayer->getGameStats()->getMaxDp()->getCurrent());
		if (!player.equals(*creature))
			sendInfo(player, name(*targetPlayer) + "'s DP have been fully refreshed.");
	} else if (equalsIgnoreCase(params[0], "fp") && targetPlayer != nullptr) {
		targetPlayer->getLifeStats()->setCurrentFp(targetPlayer->getLifeStats()->getMaxFp());
		if (!player.equals(*creature))
			sendInfo(player, name(*targetPlayer) + "'s flight time has been fully refreshed.");
	} else if (equalsIgnoreCase(params[0], "repose") && targetPlayer != nullptr) {
		runtime::Ptr<PlayerCommonData> pcd = targetPlayer->getCommonData();
		pcd->setCurrentReposeEnergy(pcd->getMaxReposeEnergy());
		PacketSendUtility::sendPacket(*targetPlayer, SM_STATUPDATE_EXP(pcd->getExpShown(), pcd->getExpRecoverable(), pcd->getExpNeed(),
														 pcd->getCurrentReposeEnergy(), pcd->getMaxReposeEnergy()));
		if (!player.equals(*creature))
			sendInfo(player, name(*targetPlayer) + "'s Energy of Repose has been fully refreshed.");
	} else {
		int32_t value;
		if (params[0].ends_with("%")) { // parity= if (params[0].endsWith("%")) {
			// Java: Integer.parseInt(params[0], 0, params[0].length() - 1, 10); its errors carry "Error at index ..." (or "" for an empty
			// number), which ChatCommand.toErrorMessage answers with "Invalid number." (only "For input string: " messages are quoted)
			int32_t hpPercent;
			try {
				hpPercent = commons::utils::parseInt(std::string_view(params[0]).substr(0, params[0].size() - 1)); // parity= int hpPercent = Integer.parseInt(params[0], 0, params[0].length() - 1, 10);
			} catch (const commons::utils::NumberFormatException&) { // parity: Java's NumberFormatException message of parseInt(s, begin, end, radix), docs/deviations/C1.md
				throw commons::utils::NumberFormatException(params[0].size() == 1 ? "" : "Error at index 0 in: \"" + params[0] + "\""); // parity: (continued)
			}
			int32_t maxHp = creature->getLifeStats()->getMaxHp(); // parity: Java reads getMaxHp() twice, in the statement of line 83
			// Java: Math.clamp((int) (hpPercent / 100f * maxHp), 0, maxHp)
			value = std::clamp(geoEngine::math::JavaFloat::doubleToInt(static_cast<float>(hpPercent) / 100.0f * static_cast<float>(maxHp)), 0, maxHp); // parity= value = Math.clamp((int) (hpPercent / 100f * creature.getLifeStats().getMaxHp()), 0, creature.getLifeStats().getMaxHp());
		} else
			value = commons::utils::parseInt(params[0]);
		creature->getLifeStats()->increaseHp(TYPE::HP, value);
		if (!player.equals(*creature))
			sendInfo(player, name(*creature) + " has been healed by " + std::to_string(value) + " health points.");
	}
}

} // namespace aion::gameserver::handlers::admincommands
