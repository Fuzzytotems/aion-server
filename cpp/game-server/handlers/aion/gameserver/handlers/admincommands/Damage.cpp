#include "aion/gameserver/handlers/admincommands/Damage.h"

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

AION_ADMIN_COMMAND(Damage);

Damage::Damage() : AdminCommand("damage") {
}

// Java Damage.java:26-107
void Damage::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	using TYPE = network::aion::serverpackets::SM_ATTACK_STATUS::TYPE; // parity: the C++ alias of Java's import of SM_ATTACK_STATUS.TYPE
	using LOG = network::aion::serverpackets::SM_ATTACK_STATUS::LOG; // parity: the C++ alias of Java's import of SM_ATTACK_STATUS.LOG
	if (params.size() < 1 || params.size() > 2) {
		info(admin, std::nullopt);
		return;
	}

	runtime::Ptr<VisibleObject> target = admin.getTarget();
	if (target == nullptr)
		PacketSendUtility::sendMessage(admin, "No target selected");
	else if (runtime::Ptr<Creature> creature = runtime::as<Creature>(target)) { // parity= else if (target instanceof Creature) { Creature creature = (Creature) target;
		int32_t dmg;
		std::string damageType = "hp";
		bool isPercent = false;

		if (equalsIgnoreCase(params[0], "mp")) {
			damageType = "mp";
		} else if (equalsIgnoreCase(params[0], "fp")) {
			damageType = "fp";
		} else if (equalsIgnoreCase(params[0], "dp")) {
			damageType = "dp";
		}

		runtime::Ptr<Player> player = runtime::as<Player>(creature);
		if (equalsIgnoreCase(damageType, "fp") || equalsIgnoreCase(damageType, "dp")) {
			if (player == nullptr) {
				info(admin, std::nullopt);
				return;
			}
		}

		if (!equalsIgnoreCase(damageType, "hp") && params.size() != 2) {
			info(admin, std::nullopt);
			return;
		}

		try {
			std::string percent = params[0];
			if (!equalsIgnoreCase(damageType, "hp"))
				percent = params[1];
			// Java: Pattern.compile("([^%]+)%").matcher(percent).find() - the first run of non-'%' characters that a '%' ends
			std::optional<std::string> group; // parity= Pattern damage = Pattern.compile("([^%]+)%"); Matcher result = damage.matcher(percent); // the scan of docs/deviations/C1.md
			for (size_t start = 0; start < percent.size() && !group; start++) { // parity: (continued)
				size_t end = start; // parity: (continued)
				while (end < percent.size() && percent[end] != '%') // parity: (continued)
					end++; // parity: (continued)
				if (end > start && end < percent.size()) // parity: (continued)
					group = percent.substr(start, end - start); // parity: (continued)
			} // parity: (continued)
			if (group) { // parity= if (result.find()) {
				dmg = commons::utils::parseInt(*group); // parity= dmg = Integer.parseInt(result.group(1));
				isPercent = true;
			} else if (equalsIgnoreCase(damageType, "hp"))
				dmg = commons::utils::parseInt(params[0]);
			else
				dmg = commons::utils::parseInt(params[1]);

			if (dmg <= 100)
				isPercent = true;

			using geoEngine::math::JavaFloat;
			if (damageType == "hp") { // parity= switch (damageType) { case "hp":
				if (isPercent)
					dmg = JavaFloat::doubleToInt(static_cast<float>(dmg) / 100.0f * static_cast<float>(creature->getLifeStats()->getMaxHp())); // parity= dmg = (int) (dmg / 100f * creature.getLifeStats().getMaxHp());
				creature->getController().onAttack(*creature, dmg, std::nullopt);
			} else if (damageType == "mp") { // parity= case "mp":
				if (isPercent)
					dmg = JavaFloat::doubleToInt(static_cast<float>(dmg) / 100.0f * static_cast<float>(creature->getLifeStats()->getMaxMp())); // parity= dmg = (int) (dmg / 100f * creature.getLifeStats().getMaxMp());
				creature->getLifeStats()->reduceMp(TYPE::DAMAGE_MP, dmg, 0, LOG::MPATTACK);
			} else if (damageType == "fp") { // parity= case "fp":
				if (isPercent)
					dmg = JavaFloat::doubleToInt(static_cast<float>(dmg) / 100.0f * static_cast<float>(player->getLifeStats()->getMaxFp())); // parity= dmg = (int) (dmg / 100f * ((Player) creature).getLifeStats().getMaxFp());
				player->getLifeStats()->reduceFp(TYPE::FP_DAMAGE, dmg, 0, LOG::FPATTACK);
			} else if (damageType == "dp") { // parity= case "dp":
				if (isPercent)
					dmg = JavaFloat::doubleToInt(static_cast<float>(dmg) / 100.0f * 4000.0f); // parity= dmg = (int) (dmg / 100f * 4000f);
				if (dmg > player->getCommonData()->getDp())
					dmg = player->getCommonData()->getDp();
				player->getCommonData()->setDp(player->getCommonData()->getDp() - dmg);
			}
		} catch (const std::exception&) { // parity= } catch (Exception ex) {
			info(admin, std::nullopt);
		}
	}
}

// Java Damage.java:109-113
void Damage::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax //damage (mp/fp/dp) <dmg | dmg%>\n<dmg> must be a number.\n" // parity= PacketSendUtility.sendMessage(player, "syntax //damage (mp/fp/dp) <dmg | dmg%>" + "\n<dmg> must be a number." + "\n(mp/fp/dp) is optional, leave out to use HP damage" + "\nin case of fp/dp, target must be player!");
										   "(mp/fp/dp) is optional, leave out to use HP damage\nin case of fp/dp, target must be player!"); // parity: (continued)
}

} // namespace aion::gameserver::handlers::admincommands
