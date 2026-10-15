#include "aion/gameserver/handlers/ai/instance/kamarBf/KamarCannonsAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

AION_AI(KamarCannonsAI, "kamarcannons");

// Java KamarCannonsAI.java:27-46
void KamarCannonsAI::handleUseItemFinish(Player& player) {
	switch (getOwner().getNpcId()) {
		case 701806:
		case 701902:
			SkillEngine::getInstance().applyEffectDirectly(21409, player, player);
			AIActions::deleteOwner(*this);
			break;
		case 701808:
		case 701912: {
			runtime::Ptr<Item> siegePower = player.getInventory().getFirstItemByItemId(164000262);
			if (siegePower != nullptr && siegePower->getItemCount() >= 1) {
				player.getInventory().decreaseByItemId(164000262, 1);
				SkillEngine::getInstance().applyEffectDirectly(player.getRace() == Race::ELYOS ? 21403 : 21404, player, player);
				AIActions::deleteOwner(*this);
			} else
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_IDKAMAR_CANT_USE_SEIGEWEAPON());
			break;
		}
	}
}

// Java KamarCannonsAI.java:48-54
void KamarCannonsAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	int32_t npcId = getOwner().getNpcId();
	if (npcId != 701806 && npcId != 701902)
		flag.set(runtime::cast<Npc>(spawn(npcId == 701808 ? 801961 : 801960, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
}

// Java KamarCannonsAI.java:56-61
void KamarCannonsAI::handleDespawned() {
	ActionItemNpcAI::handleDespawned();
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

// Java KamarCannonsAI.java:63-68
void KamarCannonsAI::handleDied() {
	ActionItemNpcAI::handleDied();
	if (runtime::Ptr<Npc> f = flag.get(); f != nullptr)
		f->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
