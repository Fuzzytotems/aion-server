#include "aion/gameserver/handlers/ai/instance/engulfedOphidianBridgeInstance/RaceGeneralsAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance {

AION_AI(RaceGeneralsAI, "engulfedophidiangenerals");

// Java RaceGeneralsAI.java:26-29
void RaceGeneralsAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java RaceGeneralsAI.java:31-119
bool RaceGeneralsAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	switch (getOwner().getNpcId()) {
		case 701989: // asmo
			switch (dialogActionId) {
				case model::DialogAction::SETPRO1:
					deleteNpcs(instance->getNpcs({701987}));
					deleteNpcs(instance->getNpcs({701985}));
					spawn(233495, 678.5313f, 471.29727f, 599.6582f, int8_t{116});
					spawn(233493, 677.62946f, 468.9142f, 599.625f, int8_t{116});
					spawn(233493, 678.6859f, 473.77505f, 599.6679f, int8_t{116});
					PacketSendUtility::broadcastToMap(getOwner(), 1402060);
					AIActions::deleteOwner(*this);
					break;
				case model::DialogAction::SETPRO2:
					deleteNpcs(instance->getNpcs({701987}));
					deleteNpcs(instance->getNpcs({701985}));
					spawn(233495, 519.66113f, 446.24088f, 620.125f, int8_t{116});
					spawn(233493, 517.5026f, 444.2977f, 620.125f, int8_t{116});
					spawn(233493, 518.73016f, 449.66315f, 620.22894f, int8_t{116});
					PacketSendUtility::broadcastToMap(getOwner(), 1402055);
					AIActions::deleteOwner(*this);
					break;
				case model::DialogAction::SETPRO3:
					deleteNpcs(instance->getNpcs({701987}));
					deleteNpcs(instance->getNpcs({701985}));
					spawn(233495, 603.4207f, 538.19196f, 590.976f, int8_t{28});
					spawn(233493, 600.8837f, 538.39116f, 591.0416f, int8_t{28});
					spawn(233493, 605.8344f, 538.0445f, 590.99445f, int8_t{28});
					PacketSendUtility::broadcastToMap(getOwner(), 1402070);
					AIActions::deleteOwner(*this);
					break;
				case model::DialogAction::SETPRO4:
					deleteNpcs(instance->getNpcs({701987}));
					deleteNpcs(instance->getNpcs({701985}));
					spawn(233495, 481.47342f, 526.27606f, 597.375f, int8_t{19});
					spawn(233493, 479.6977f, 528.80994f, 597.375f, int8_t{19});
					spawn(233493, 481.879f, 523.497f, 597.49243f, int8_t{19});
					PacketSendUtility::broadcastToMap(getOwner(), 1402065);
					AIActions::deleteOwner(*this);
					break;
			}
			break;
		default: // elyos
			switch (dialogActionId) {
				case model::DialogAction::SETPRO1:
					deleteNpcs(instance->getNpcs({701986}));
					deleteNpcs(instance->getNpcs({701984}));
					spawn(233494, 691.1125f, 467.0932f, 599.875f, int8_t{54});
					spawn(233492, 690.11017f, 464.7947f, 599.875f, int8_t{54});
					spawn(233492, 691.4471f, 471.80127f, 599.84045f, int8_t{54});
					AIActions::deleteOwner(*this);
					PacketSendUtility::broadcastToMap(getOwner(), 1402060);
					break;
				case model::DialogAction::SETPRO2:
					deleteNpcs(instance->getNpcs({701986}));
					deleteNpcs(instance->getNpcs({701984}));
					spawn(233494, 531.18066f, 446.37927f, 620.25f, int8_t{58});
					spawn(233492, 532.5186f, 444.30832f, 620.25f, int8_t{58});
					spawn(233492, 532.3772f, 449.40405f, 620.25f, int8_t{58});
					AIActions::deleteOwner(*this);
					PacketSendUtility::broadcastToMap(getOwner(), 1402055);
					break;
				case model::DialogAction::SETPRO3:
					deleteNpcs(instance->getNpcs({701986}));
					deleteNpcs(instance->getNpcs({701984}));
					spawn(233494, 618.9949f, 551.4716f, 590.75f, int8_t{55});
					spawn(233492, 621.22687f, 555.14624f, 590.67834f, int8_t{55});
					spawn(233492, 620.4509f, 547.35284f, 590.75f, int8_t{55});
					AIActions::deleteOwner(*this);
					PacketSendUtility::broadcastToMap(getOwner(), 1402070);
					break;
				case model::DialogAction::SETPRO4:
					deleteNpcs(instance->getNpcs({701986}));
					deleteNpcs(instance->getNpcs({701984}));
					spawn(233494, 478.23563f, 543.6911f, 597.5f, int8_t{112});
					spawn(233492, 479.92032f, 545.8376f, 597.5f, int8_t{112});
					spawn(233492, 476.69766f, 542.24774f, 597.5f, int8_t{112});
					AIActions::deleteOwner(*this);
					PacketSendUtility::broadcastToMap(getOwner(), 1402065);
					break;
			}
			break;
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java RaceGeneralsAI.java:121-127
void RaceGeneralsAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr) {
			npc->getController().delete_();
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance
