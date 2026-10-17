#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombImperialShrineAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombImperialShrineAI, "shugo_tomb_imperial_shrine");

// Java ShugoTombImperialShrineAI.java:34-39
ShugoTombImperialShrineAI::ShugoTombImperialShrineAI(Npc& owner) : GeneralNpcAI(owner) {
	entriesByKeyId.put(GOLD_KEY, 0);
	entriesByKeyId.put(SILVER_KEY, 0);
	entriesByKeyId.put(BRONZE_KEY, 0);
}

// Java ShugoTombImperialShrineAI.java:41-44
void ShugoTombImperialShrineAI::handleDialogStart(Player& player) {
	GeneralNpcAI::handleDialogStart(player);
}

// Java ShugoTombImperialShrineAI.java:46-55
bool ShugoTombImperialShrineAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, dialogActionId);
	env->setExtendedRewardIndex(extendedRewardIndex);
	switch (dialogActionId) {
		case model::DialogAction::SETPRO1:
		case model::DialogAction::SETPRO2:
		case model::DialogAction::SETPRO3:
			enterTreasureRoomIfPossible(player, dialogActionId);
			break;
		default:
			QuestEngine::getInstance().onDialog(*env);
			break;
	}
	return true;
}

// Java ShugoTombImperialShrineAI.java:57-98
void ShugoTombImperialShrineAI::enterTreasureRoomIfPossible(Player& player, int32_t dialogActionId) {
	runtime::Ptr<WorldMapInstance> wmi = getPosition()->getWorldMapInstance();
	runtime::Ref<Point3D> destination = nullptr;
	switch (dialogActionId) {
		case model::DialogAction::SETPRO1: // Emperor treasure room
			if (!player.getInventory().decreaseByItemId(GOLD_KEY, 1)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_IDEVENT01_GOLD_MAP());
				return;
			}
			switch (getEntryCountByKeyId(GOLD_KEY)) {
				case 0:
					destination = Point3D::create(74.292816f, 350.66525f, 285.14545f);
					break;
				case 1:
					destination = Point3D::create(375.25629f, 198.02574f, 306.84357f);
					break;
				default:
					destination = Point3D::create(335.55219f, 334.60947f, 458.5939f);
					break;
			}
			break;
		case model::DialogAction::SETPRO2: // Empress treasure room
			if (!player.getInventory().decreaseByItemId(SILVER_KEY, 1)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_IDEVENT01_SILVER_MAP());
				return;
			}
			switch (getEntryCountByKeyId(SILVER_KEY)) {
				case 0:
					destination = Point3D::create(347.96069f, 41.424923f, 358.3918f);
					break;
				case 1:
					destination = Point3D::create(82.877762f, 240.61363f, 421.79004f);
					break;
				default:
					destination = Point3D::create(75.203018f, 432.58603f, 455.82312f);
					break;
			}
			break;
		case model::DialogAction::SETPRO3: // Prince treasure room
			if (!player.getInventory().decreaseByItemId(BRONZE_KEY, 1)) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_IDEVENT01_BRONZE_MAP());
				return;
			}
			switch (getEntryCountByKeyId(BRONZE_KEY)) {
				case 0:
					destination = Point3D::create(409.74783f, 247.3778f, 516.4457f);
					break;
				case 1:
					destination = Point3D::create(181.56918f, 385.08932f, 616.1734f);
					break;
				default:
					destination = Point3D::create(177.63902f, 77.778755f, 466.1734f);
					break;
			}
			break;
		default:
			break;
	}
	if (destination != nullptr)
		TeleportService::teleportTo(player, *wmi, destination->getX(), destination->getY(), destination->getZ(), int8_t{0}, TeleportAnimation::FADE_OUT_BEAM);
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
}

// Java ShugoTombImperialShrineAI.java:100-104 (synchronized)
int32_t ShugoTombImperialShrineAI::getEntryCountByKeyId(int32_t keyId) {
	SYNCHRONIZED(monitor) {
		int32_t entryCount = *entriesByKeyId.get(keyId);
		entriesByKeyId.put(keyId, entryCount + 1);
		return entryCount;
	}
	return 0; // unreachable: SYNCHRONIZED runs its block once
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
