#include "aion/gameserver/handlers/admincommands/Morph.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Morph);

Morph::Morph()
	: AdminCommand("morph", "Morphs a player into any NPC.",
		  " - morphs you into the NPC you are targeting.\n"
		  "<id> - Morphs your target into the specified NPC (0 to cancel).\n") {
}

// Java Morph.java:24-55
void Morph::execute(Player& admin, std::span<const std::string> params) {
	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(admin.getTarget());
	Player& target = targetPlayer != nullptr ? *targetPlayer : admin;
	const NpcTemplate* npcTemplate;
	if (params.empty()) {
		if (admin.getTarget() == nullptr || admin.equals(*admin.getTarget())) {
			sendInfo(admin);
			return;
		}
		const auto* t = dynamic_cast<const NpcTemplate*>(admin.getTarget()->getObjectTemplate());
		if (t == nullptr) {
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
			return;
		}
		npcTemplate = t;
	} else {
		int32_t modelId = commons::utils::parseInt(params[0]);
		if (modelId == 0) {
			target.getTransformModel().apply(0);
			sendInfo(admin, "Cancelled" + (target.equals(admin) ? std::string() : " " + name(target) + "'s") + " morph.");
			return;
		}
		npcTemplate = DataManager::NPC_DATA->getNpcTemplate(modelId);
		if (npcTemplate == nullptr) {
			sendInfo(admin, "Invalid ID.");
			return;
		}
	}
	target.getTransformModel().apply(npcTemplate->getTemplateId());
	sendInfo(admin, "You morphed" + (target.equals(admin) ? std::string() : " " + name(target)) + " into " + npcTemplate->getL10n() + ".");
	if (!target.equals(admin))
		sendInfo(target, name(admin) + " morphed you into " + npcTemplate->getL10n() + ".");
}

} // namespace aion::gameserver::handlers::admincommands
