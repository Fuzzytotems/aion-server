#include "aion/gameserver/handlers/instance/pvparenas/HarmonyTrainingGroundsInstance.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/instance/instancescore/HarmonyArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/HarmonyGroupReward.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/HarmonyScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::pvparenas {

AION_INSTANCE_HANDLER(HarmonyTrainingGroundsInstance, 300570000);

// Java HarmonyTrainingGroundsInstance.java:29-31
HarmonyTrainingGroundsInstance::HarmonyTrainingGroundsInstance(WorldMapInstance& instance) : PvPArenaInstance(instance) {
}

runtime::Ref<HarmonyTrainingGroundsInstance> HarmonyTrainingGroundsInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<HarmonyTrainingGroundsInstance>(instance);
}

// Java HarmonyTrainingGroundsInstance.java:33-39
int32_t HarmonyTrainingGroundsInstance::getBoostMoraleEffectDuration(int32_t rank) {
	switch (rank) {
		case 0:
			return 14000;
		default:
			return 15000;
	}
}

// Java HarmonyTrainingGroundsInstance.java:41-115
void HarmonyTrainingGroundsInstance::spawnRings() {
	spawnFlyRing("PVP_ARENA_1", *Point3D::create(526.3772f, 1800.0292f, 176.74919f), *Point3D::create(526.3772f, 1800.0292f, 179.74919f),
		*Point3D::create(525.0156f, 1797.356f, 176.74919f));
	spawnFlyRing("PVP_ARENA_2", *Point3D::create(506.68332f, 1801.1233f, 176.29509f), *Point3D::create(506.68332f, 1801.1233f, 179.29509f),
		*Point3D::create(506.68332f, 1798.1233f, 176.29509f));
	spawnFlyRing("PVP_ARENA_3", *Point3D::create(537.7089f, 1772.1154f, 176.39908f), *Point3D::create(537.7089f, 1772.1154f, 179.39908f),
		*Point3D::create(534.8185f, 1772.9186f, 176.39908f));
	spawnFlyRing("PVP_ARENA_4", *Point3D::create(506.82697f, 1761.2292f, 176.5923f), *Point3D::create(506.82697f, 1761.2292f, 179.5923f),
		*Point3D::create(506.54703f, 1764.2162f, 176.5923f));
	spawnFlyRing("PVP_ARENA_5", *Point3D::create(526.93854f, 1761.6971f, 176.46439f), *Point3D::create(526.93854f, 1761.6971f, 179.46439f),
		*Point3D::create(525.2771f, 1764.1951f, 176.46439f));
	spawnFlyRing("PVP_ARENA_6", *Point3D::create(486.66653f, 1761.2954f, 176.52344f), *Point3D::create(486.66653f, 1761.2954f, 179.52344f),
		*Point3D::create(486.22333f, 1764.2625f, 176.52344f));
	spawnFlyRing("PVP_ARENA_7", *Point3D::create(463.7616f, 1760.8594f, 176.37796f), *Point3D::create(463.7616f, 1760.8594f, 179.37796f),
		*Point3D::create(465.9929f, 1762.8647f, 176.37796f));
	spawnFlyRing("PVP_ARENA_8", *Point3D::create(453.18936f, 1775.0388f, 176.37965f), *Point3D::create(453.18936f, 1775.0388f, 179.37965f),
		*Point3D::create(451.03714f, 1772.9489f, 176.37965f));
	spawnFlyRing("PVP_ARENA_9", *Point3D::create(453.36063f, 1792.1831f, 176.28424f), *Point3D::create(453.36063f, 1792.1831f, 179.28424f),
		*Point3D::create(450.8542f, 1792.6652f, 176.28424f));
	spawnFlyRing("PVP_ARENA_10", *Point3D::create(464.15717f, 1801.1119f, 176.49224f), *Point3D::create(464.15717f, 1801.1119f, 179.4922f),
		*Point3D::create(462.10913f, 1803.3041f, 176.49224f));
	spawnFlyRing("PVP_ARENA_11", *Point3D::create(486.1619f, 1801.2965f, 176.45914f), *Point3D::create(486.1619f, 1801.2965f, 179.45914f),
		*Point3D::create(485.39664f, 1804.1973f, 176.45914f));
	spawnFlyRing("PVP_ARENA_12", *Point3D::create(537.2858f, 1789.3806f, 176.13591f), *Point3D::create(537.2858f, 1789.3806f, 179.13591f),
		*Point3D::create(534.82043f, 1787.6713f, 176.13591f));
	spawnFlyRing("PVP_ARENA_13", *Point3D::create(519.96678f, 1767.7379f, 164.51219f), *Point3D::create(519.96678f, 1767.7379f, 167.51219f),
		*Point3D::create(518.04193f, 1770.0391f, 164.51219f));
	spawnFlyRing("PVP_ARENA_14", *Point3D::create(529.45605f, 1780.3438f, 152.91333f), *Point3D::create(529.45605f, 1780.3438f, 155.91333f),
		*Point3D::create(526.61993f, 1779.3657f, 152.91333f));
	spawnFlyRing("PVP_ARENA_15", *Point3D::create(520.3683f, 1792.2515f, 138.297f), *Point3D::create(520.3683f, 1792.2515f, 141.297f),
		*Point3D::create(520.271f, 1789.253f, 138.297f));
	spawnFlyRing("PVP_ARENA_16", *Point3D::create(469.53625f, 1792.6782f, 163.97906f), *Point3D::create(469.53625f, 1792.6782f, 166.97906f),
		*Point3D::create(467.26062f, 1794.633f, 163.97906f));
	spawnFlyRing("PVP_ARENA_17", *Point3D::create(459.51752f, 1783.4421f, 152.58247f), *Point3D::create(459.51752f, 1783.4421f, 155.58247f),
		*Point3D::create(456.87985f, 1782.013f, 152.58247f));
	spawnFlyRing("PVP_ARENA_18", *Point3D::create(469.3592f, 1769.092f, 137.91689f), *Point3D::create(469.3592f, 1769.092f, 140.91689f),
		*Point3D::create(470.37436f, 1771.915f, 137.91689f));
	spawnFlyRing("PVP_ARENA_19", *Point3D::create(494.79196f, 1759.603f, 151.13585f), *Point3D::create(494.79196f, 1759.603f, 154.13585f),
		*Point3D::create(494.51202f, 1762.59f, 151.13585f));
	spawnFlyRing("PVP_ARENA_20", *Point3D::create(503.06744f, 1759.9893f, 157.17598f), *Point3D::create(503.06744f, 1759.9893f, 160.17598f),
		*Point3D::create(502.7875f, 1762.9762f, 157.17598f));
	spawnFlyRing("PVP_ARENA_21", *Point3D::create(495.112f, 1802.11895f, 151.642f), *Point3D::create(495.112f, 1802.11895f, 154.642f),
		*Point3D::create(494.50687f, 1805.1278f, 151.642f));
	spawnFlyRing("PVP_ARENA_22", *Point3D::create(503.28552f, 1801.8163f, 142.87953f), *Point3D::create(503.28552f, 1801.8163f, 145.87953f),
		*Point3D::create(502.52026f, 1804.717f, 142.87953f));
	spawnFlyRing("PVP_ARENA_23", *Point3D::create(487.70947f, 1802.1512f, 157.30313f), *Point3D::create(487.70947f, 1802.1512f, 160.30313f),
		*Point3D::create(487.10434f, 1805.0896f, 157.30313f));
	spawnFlyRing("PVP_ARENA_24", *Point3D::create(486.5052f, 1759.7152f, 143.67566f), *Point3D::create(486.5052f, 1759.7152f, 146.67566f),
		*Point3D::create(485.90005f, 1762.6536f, 143.67566f));
	spawnFlyRing("PVP_ARENA_25", *Point3D::create(495.1142f, 1769.1791f, 151.02248f), *Point3D::create(495.1142f, 1769.1791f, 154.02248f),
		*Point3D::create(492.2781f, 1768.201f, 151.02248f));
	spawnFlyRing("PVP_ARENA_26", *Point3D::create(517.1142f, 1780.5552f, 151.54797f), *Point3D::create(517.1142f, 1780.5552f, 154.54797f),
		*Point3D::create(517.248f, 1783.5222f, 151.54797f));
	spawnFlyRing("PVP_ARENA_27", *Point3D::create(539.5928f, 1768.1559f, 153.20947f), *Point3D::create(539.5928f, 1768.1559f, 156.20947f),
		*Point3D::create(536.7507f, 1769.1163f, 153.20947f));
	spawnFlyRing("PVP_ARENA_28", *Point3D::create(494.78378f, 1791.6841f, 151.01425f), *Point3D::create(494.78378f, 1791.6841f, 154.01425f),
		*Point3D::create(497.77554f, 1791.9062f, 151.01425f));
	spawnFlyRing("PVP_ARENA_29", *Point3D::create(496.37292f, 1781.1427f, 151.12042f), *Point3D::create(496.37292f, 1781.1427f, 154.12042f),
		*Point3D::create(496.093f, 1784.1296f, 151.12042f));
	spawnFlyRing("PVP_ARENA_30", *Point3D::create(504.7736f, 1781.0814f, 151.68193f), *Point3D::create(504.7736f, 1781.0814f, 154.68193f),
		*Point3D::create(505.331f, 1778.1337f, 151.68193f));
	spawnFlyRing("PVP_ARENA_31", *Point3D::create(538.9759f, 1791.9437f, 153.94568f), *Point3D::create(538.9759f, 1791.9437f, 156.94568f),
		*Point3D::create(537.32996f, 1789.4355f, 153.94568f));
	spawnFlyRing("PVP_ARENA_32", *Point3D::create(486.21588f, 1781.1268f, 150.98273f), *Point3D::create(486.21588f, 1781.1268f, 153.98273f),
		*Point3D::create(486.6109f, 1778.0f, 150.98273f));
	spawnFlyRing("PVP_ARENA_33", *Point3D::create(472.12088f, 1781.8315f, 150.49611f), *Point3D::create(472.12088f, 1781.8315f, 153.49611f),
		*Point3D::create(472.839f, 1778.9187f, 150.49611f));
	spawnFlyRing("PVP_ARENA_34", *Point3D::create(496.78357f, 1781.3253f, 151.18472f), *Point3D::create(496.78357f, 1781.3253f, 154.18472f),
		*Point3D::create(496.64036f, 1784.2924f, 151.18472f));
	spawnFlyRing("PVP_ARENA_35", *Point3D::create(450.68466f, 1771.1696f, 153.57443f), *Point3D::create(450.68466f, 1771.1696f, 156.57443f),
		*Point3D::create(452.91595f, 1771.1696f, 153.57443f));
	spawnFlyRing("PVP_ARENA_36", *Point3D::create(452.1979f, 1790.3647f, 153.43126f), *Point3D::create(452.1979f, 1790.3647f, 156.43126f),
		*Point3D::create(449.41272f, 1791.4795f, 153.43126f));
}

// Java HarmonyTrainingGroundsInstance.java:117-119 (the template is allocated once and never freed, like static data: FlyRingTemplate.h)
void HarmonyTrainingGroundsInstance::spawnFlyRing(std::string_view name, Point3D& center, Point3D& p1, Point3D& p2) {
	VisibleObject::create<FlyRing>(new FlyRingTemplate(name, mapId, center, p1, p2, 3), instance->getInstanceId())->spawn();
}

// Java HarmonyTrainingGroundsInstance.java:121-171
bool HarmonyTrainingGroundsInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (!instanceScore.get()->isStartProgress())
		return false;
	runtime::Ptr<Npc> npc = nullptr;
	if (flyingRing == "PVP_ARENA_1")
		npc = getNpc(526.5524f, 1799.9530f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_2")
		npc = getNpc(506.4008f, 1801.0159f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_3")
		npc = getNpc(537.6169f, 1772.0968f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_4")
		npc = getNpc(506.2996f, 1761.2419f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_5")
		npc = getNpc(526.5186f, 1761.3792f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_6")
		npc = getNpc(485.9503f, 1761.1323f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_7")
		npc = getNpc(463.6774f, 1761.2948f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_8")
		npc = getNpc(453.2310f, 1774.9258f, 177.3975f);
	else if (flyingRing == "PVP_ARENA_9")
		npc = getNpc(453.3799f, 1791.6423f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_10")
		npc = getNpc(464.1622f, 1801.0581f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_11")
		npc = getNpc(485.8056f, 1801.1987f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_12")
		npc = getNpc(537.3194f, 1789.4381f, 177.3270f);
	else if (flyingRing == "PVP_ARENA_13")
		npc = getNpc(520.1588f, 1767.9170f, 165.3259f);
	else if (flyingRing == "PVP_ARENA_14")
		npc = getNpc(529.5792f, 1780.8058f, 153.6571f);
	else if (flyingRing == "PVP_ARENA_15")
		npc = getNpc(519.9453f, 1792.4106f, 139.4744f);
	else if (flyingRing == "PVP_ARENA_16")
		npc = getNpc(469.7408f, 1792.6573f, 165.4409f);
	else if (flyingRing == "PVP_ARENA_17")
		npc = getNpc(459.6954f, 1783.1649f, 153.1804f);
	else if (flyingRing == "PVP_ARENA_18")
		npc = getNpc(469.6530f, 1769.2192f, 138.8079f);
	else if (flyingRing == "PVP_ARENA_19")
		npc = getNpc(494.7642f, 1759.5282f, 152.5068f);
	else if (flyingRing == "PVP_ARENA_20")
		npc = getNpc(503.3351f, 1759.6985f, 158.4491f);
	else if (flyingRing == "PVP_ARENA_21")
		npc = getNpc(494.9348f, 1802.1798f, 152.5857f);
	else if (flyingRing == "PVP_ARENA_22")
		npc = getNpc(503.2661f, 1801.7520f, 143.9769f);
	else if (flyingRing == "PVP_ARENA_23")
		npc = getNpc(486.9509f, 1801.9956f, 158.4124f);
	else if (flyingRing == "PVP_ARENA_24")
		npc = getNpc(486.7321f, 1759.7345f, 144.8943f);
	else if (flyingRing == "PVP_ARENA_25")
		npc = getNpc(495.0289f, 1769.2734f, 152.1635f);
	else if (flyingRing == "PVP_ARENA_26")
		npc = getNpc(518.0588f, 1780.5404f, 152.5605f);
	else if (flyingRing == "PVP_ARENA_27")
		npc = getNpc(539.5499f, 1767.9496f, 154.2043f);
	else if (flyingRing == "PVP_ARENA_28")
		npc = getNpc(494.9674f, 1791.8362f, 152.1635f);
	else if (flyingRing == "PVP_ARENA_29")
		npc = getNpc(495.0894f, 1781.0751f, 152.1635f);
	else if (flyingRing == "PVP_ARENA_30")
		npc = getNpc(505.1635f, 1781.0885f, 152.3523f);
	else if (flyingRing == "PVP_ARENA_31")
		npc = getNpc(539.3393f, 1791.3956f, 154.9766f);
	else if (flyingRing == "PVP_ARENA_32")
		npc = getNpc(486.1324f, 1781.1040f, 152.0575f);
	else if (flyingRing == "PVP_ARENA_33")
		npc = getNpc(472.1582f, 1781.9821f, 151.6493f);
	else if (flyingRing == "PVP_ARENA_34")
		npc = getNpc(496.5575f, 1781.0718f, 152.1614f);
	else if (flyingRing == "PVP_ARENA_35")
		npc = getNpc(450.1526f, 1771.6342f, 154.4948f);
	else if (flyingRing == "PVP_ARENA_36")
		npc = getNpc(451.9798f, 1789.6945f, 154.4945f);
	if (npc != nullptr && npc->isSpawned()) {
		npc->getController().deleteAndScheduleRespawn();
		runtime::Ptr<HarmonyGroupReward> reward = harmonyScore().getGroupReward(player.getObjectId());
		updatePoints(reward, player, *npc, 100);
	}
	return false;
}

// Java HarmonyTrainingGroundsInstance.java:173-176
void HarmonyTrainingGroundsInstance::updatePoints(runtime::Ptr<PvPArenaPlayerReward> receiver, Player& player, VisibleObject& victim,
                                                  int32_t rewardPoints) {
	PvPArenaInstance::updatePoints(receiver, player, victim, rewardPoints);
}

// Java HarmonyTrainingGroundsInstance.java:178-183
void HarmonyTrainingGroundsInstance::onInstanceCreate() {
	pointsPerKill = 800;
	pointsPerDeath = -150;
	PvPArenaInstance::onInstanceCreate();
}

// Java HarmonyTrainingGroundsInstance.java:185-188
float HarmonyTrainingGroundsInstance::getRunnerUpScoreMod(int32_t victimRank) {
	return 4.0f;
}

// Java HarmonyTrainingGroundsInstance.java:190-193
runtime::Ref<PvPArenaScore> HarmonyTrainingGroundsInstance::createNewArenaScore() {
	return HarmonyArenaScore::create(*instance);
}

// Java HarmonyTrainingGroundsInstance.java:195-198
std::vector<runtime::Ptr<PvPArenaPlayerReward>> HarmonyTrainingGroundsInstance::getArenaRewards() {
	std::vector<runtime::Ptr<PvPArenaPlayerReward>> rewards;
	for (const runtime::Ptr<HarmonyGroupReward>& group : harmonyScore().getHarmonyGroupInside())
		rewards.emplace_back(group);
	return rewards;
}

// Java HarmonyTrainingGroundsInstance.java:200-203
runtime::Ptr<PvPArenaPlayerReward> HarmonyTrainingGroundsInstance::getStatReward(Player& player) {
	return harmonyScore().getGroupReward(player.getObjectId());
}

// Java HarmonyTrainingGroundsInstance.java:205-208
bool HarmonyTrainingGroundsInstance::canStart() {
	return harmonyScore().getHarmonyGroupInside().size() > 1;
}

// Java HarmonyTrainingGroundsInstance.java:210-213
bool HarmonyTrainingGroundsInstance::shouldMergeGroupDamage() {
	return true;
}

// Java HarmonyTrainingGroundsInstance.java:215-221
void HarmonyTrainingGroundsInstance::sendEntryPacket(Player& player) {
	broadcastUpdate(player, InstanceScoreType::UPDATE_RANK);
	broadcastUpdate(player, InstanceScoreType::INIT_PLAYER);
	broadcastUpdate(player, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS);
	broadcastUpdate(InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE);
}

// Java HarmonyTrainingGroundsInstance.java:223-227
void HarmonyTrainingGroundsInstance::broadcastUpdate(Player& target, InstanceScoreType scoreType) {
	runtime::Ref<HarmonyScoreWriter> writer = HarmonyScoreWriter::create(harmonyScore(), scoreType, target);
	PacketSendUtility::broadcastToMap(*instance, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

// Java HarmonyTrainingGroundsInstance.java:229-233
void HarmonyTrainingGroundsInstance::broadcastUpdate(InstanceScoreType scoreType) {
	runtime::Ref<HarmonyScoreWriter> writer = HarmonyScoreWriter::create(harmonyScore(), scoreType);
	PacketSendUtility::broadcastToMap(*instance, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

// Java HarmonyTrainingGroundsInstance.java:235-238
void HarmonyTrainingGroundsInstance::broadcastResults() {
	instance->forEachPlayer([this](Player& player) { sendPacket(runtime::Ptr<Player>(player), InstanceScoreType::SHOW_REWARD); });
}

// Java HarmonyTrainingGroundsInstance.java:240-244 (a null receiver or type: Java's NullPointerException; the base never passes them here)
void HarmonyTrainingGroundsInstance::sendPacket(runtime::Ptr<Player> receiver, std::optional<InstanceScoreType> scoreType) {
	runtime::Ref<HarmonyScoreWriter> writer = HarmonyScoreWriter::create(harmonyScore(), scoreType.value(), *receiver);
	PacketSendUtility::sendPacket(*receiver, SM_INSTANCE_SCORE(instance->getMapId(), *writer, getTime()));
}

// Java HarmonyTrainingGroundsInstance.java:246-248
int32_t HarmonyTrainingGroundsInstance::getTime() {
	return instanceScore.get()->getTime();
}

/** C++: Java's ((HarmonyArenaScore) instanceScore) */
HarmonyArenaScore& HarmonyTrainingGroundsInstance::harmonyScore() {
	return *runtime::cast<HarmonyArenaScore>(runtime::Ptr<PvPArenaScore>(instanceScore.get()));
}

} // namespace aion::gameserver::handlers::instance::pvparenas
