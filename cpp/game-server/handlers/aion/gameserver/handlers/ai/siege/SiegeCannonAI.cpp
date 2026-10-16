#include "aion/gameserver/handlers/ai/siege/SiegeCannonAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(SiegeCannonAI, "siege_cannon");

// Java SiegeCannonAI.java:25-28
void SiegeCannonAI::handleDialogStart(Player& player) {
	TalkEventHandler::onTalk(*this, player);
}

// Java SiegeCannonAI.java:30-41
bool SiegeCannonAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	static_cast<void>(questId);
	static_cast<void>(extendedRewardIndex);
	int32_t morphSkill = getMorphSkill();
	Npc& ownerNpc = getOwner(); // Java: Npc owner (C++: the name is AbstractAI's field)
	if (dialogActionId == model::DialogAction::SETPRO1 && morphSkill != 0) {
		TeleportService::teleportTo(player, ownerNpc.getWorldId(), ownerNpc.getInstanceId(), ownerNpc.getX(), ownerNpc.getY(), ownerNpc.getZ(), ownerNpc.getHeading());
		runtime::Ref<skillengine::model::Skill> skill =
			SkillEngine::getInstance().getSkill(getOwner(), morphSkill >> 8, morphSkill & 0xFF, runtime::Ptr<VisibleObject>(player));
		if (skill == nullptr) // Java: getSkill(...).useNoAnimationSkill() on null
			throw runtime::NullPointerException("SiegeCannonAI: morph skill " + std::to_string(morphSkill >> 8));
		skill->useNoAnimationSkill();
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		AIActions::deleteOwner(*this);
	}
	return true;
}

// Java SiegeCannonAI.java:43-131
int32_t SiegeCannonAI::getMorphSkill() {
	switch (getNpcId()) {
		case 251725: // Krotan Elyos - Sky Cannon
		case 251726:
		case 251727:
		case 251728:
		case 251729:
		case 251730:
		case 251731:
		case 251732:
		case 251733:
		case 251734:
		case 251745: // Kysis Elyos - Sky Cannon
		case 251746:
		case 251747:
		case 251748:
		case 251749:
		case 251750:
		case 251751:
		case 251752:
		case 251753:
		case 251754:
		case 251765: // Miren Elyos - Sky Cannon
		case 251766:
		case 251767:
		case 251768:
		case 251769:
		case 251770:
		case 251771:
		case 251772:
		case 251773:
		case 251774:
		case 882253: // Divine
		case 882255:
			return 0x540D41; // 21517 65
		case 252164: // Wealhtheow Elyos
		case 252165:
		case 252166:
		case 252167:
		case 252168:
		case 252169:
		case 252170:
			return 0x538941; // 21385 65
		case 251735: // Krotan Asmo - Sky Cannon
		case 251736:
		case 251737:
		case 251738:
		case 251739:
		case 251740:
		case 251741:
		case 251742:
		case 251743:
		case 251744:
		case 251755: // Kysis Asmo - Sky Cannon
		case 251756:
		case 251757:
		case 251758:
		case 251759:
		case 251760:
		case 251761:
		case 251762:
		case 251763:
		case 251764:
		case 251775: // Miren Asmo - Sky Cannon
		case 251776:
		case 251777:
		case 251778:
		case 251779:
		case 251780:
		case 251781:
		case 251782:
		case 251783:
		case 251784:
		case 882254: // Divine Asmo Sky Cannon
		case 882256:
			return 0x540E41; // 21518 65
		case 252171: // Wealhtheow Asmo
		case 252172:
		case 252173:
		case 252174:
		case 252175:
		case 252176:
		case 252177:
			return 0x538A41; // 21386 65
		default:
			return 0;
	}
}

// Java SiegeCannonAI.java:133-139
bool SiegeCannonAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::siege
