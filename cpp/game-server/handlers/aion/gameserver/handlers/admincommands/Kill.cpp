#include "aion/gameserver/handlers/admincommands/Kill.h"

#include <cmath>
#include <functional>
#include <typeinfo>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Kill);

Kill::Kill()
	: AdminCommand("kill", "Kills the specified NPC(s) or player.",
		  " - kills your target (can be NPC or player)\n"
		  "all [neutral|enemy|npcId] - kills all NPCs in the surrounding area (default: all, optional: only neutral/hostile NPCs/specific NPC)\n"
		  "<range (in meters)> [neutral|enemy|npcId] - kills NPCs in the specified radius around you (default: all, optional: only neutral/hostile "
		  "NPCs/specific NPC)\n") {
}

// Java Kill.java:30-88
void Kill::execute(Player& player, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	runtime::Ptr<VisibleObject> target = player.getTarget();
	if (params.size() > 2 || (params.empty() && target == nullptr)) {
		sendInfo(player);
		return;
	}

	if (params.empty()) {
		if (runtime::Ptr<Creature> creature = runtime::as<Creature>(target)) {
			std::string targetInfo = commons::utils::StringUtils::toLowerCase(utils::simpleClassName(typeid(*target))) + ": ";
			if (runtime::as<Npc>(target) != nullptr)
				targetInfo += ChatUtil::path(*target, true);
			else
				targetInfo += name(*target);
			if (kill(player, *creature))
				sendInfo(player, "Killed " + targetInfo);
			else
				sendInfo(player, "Couldn't kill " + targetInfo);
		} else {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		}
	} else {
		std::function<bool(Creature&)> filter;
		if (equalsIgnoreCase(params[0], "all")) {
			filter = [](Creature&) { return true; };
		} else {
			float range = commons::configuration::transformers::NumberParser::parseFloat(params[0]); // Java: Float.parseFloat
			if (range < 0) {
				sendInfo(player, "The given range must be larger than 0.");
				return;
			}
			// if input was integer, add 0.999 so it matches the client's displayed target distance (client doesn't round up at .5)
			// Java: range == Math.round(range) (Math.round(float): floor(x + 0.5) narrowed to int, compared as float)
			float rounded = static_cast<float>(geoEngine::math::JavaFloat::doubleToInt(std::floor(static_cast<double>(range) + 0.5)));
			float finalRange = range == rounded ? range + 0.999f : range;
			filter = [&player, finalRange](Creature& creature) { return PositionUtil::isInRange(player, creature, finalRange); };
		}
		if (params.size() == 2) {
			std::function<bool(Creature&)> base = filter;
			if (equalsIgnoreCase(params[1], "neutral")) {
				filter = [base, &player](Creature& creature) { return base(creature) && !player.isEnemy(creature); };
			} else if (equalsIgnoreCase(params[1], "enemy")) {
				filter = [base, &player](Creature& creature) { return base(creature) && player.isEnemy(creature); };
			} else {
				int32_t npcId = commons::utils::parseInt(params[1]);
				filter = [base, npcId](Creature& creature) { return base(creature) && creature.getObjectTemplate()->getTemplateId() == npcId; };
			}
		}
		int32_t count = 0; // Java: AtomicInteger
		for (const runtime::Ptr<world::knownlist::KnownObject>& o : player.getKnownList().stream()) {
			runtime::Ptr<Creature> creature = runtime::as<Creature>(o->get());
			if (creature == nullptr || runtime::as<Player>(creature) != nullptr)
				continue;
			if (!filter(*creature))
				continue;
			if (kill(player, *creature))
				count++;
		}
		sendInfo(player, std::to_string(count) + " NPC(s) were killed.");
	}
}

// Java Kill.java:90-96
bool Kill::kill(Player& attacker, Creature& target) {
	if (target.isDead() || target.getLifeStats()->isAboutToDie())
		return false;
	target.getController().onAttack(target.isPvpTarget(attacker) && !target.isEnemy(attacker) ? target : static_cast<Creature&>(attacker),
		target.getLifeStats()->getMaxHp(), std::nullopt);
	return true;
}

} // namespace aion::gameserver::handlers::admincommands
