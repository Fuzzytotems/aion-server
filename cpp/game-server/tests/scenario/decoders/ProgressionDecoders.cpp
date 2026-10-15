#include "decoders/ProgressionDecoders.h"

#include <string>

namespace aion::gameserver::scenario::decoders {

ActionAnimation decodeActionAnimation(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ACTION_ANIMATION");
	ActionAnimation animation;
	animation.objectId = reader.D();        // SM_ACTION_ANIMATION.java:28, writeD(targetObjectId)
	animation.animation = reader.H();       // :29, writeH(actionAnimation.getId())
	animation.levelOrObjectId = reader.D(); // :30, writeD(levelOrObjectId)
	reader.expectFullyConsumed();
	return animation;
}

SkillRemove decodeSkillRemove(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SKILL_REMOVE");
	SkillRemove remove;
	remove.skillId = reader.H();      // SM_SKILL_REMOVE.java:24, writeH(skillId)
	remove.levelOrFlag = reader.C();  // :25, writeC(skillLevel)
	remove.skillType = reader.C();    // :26, writeC(skillType)
	if (remove.skillType != 0 && remove.skillType != 1 && remove.skillType != 3)
		reader.fail("the skill type is " + std::to_string(remove.skillType) + ", but PlayerSkillEntry's is 0, 1 or 3 (PlayerSkillEntry.java:17)");
	reader.expectFullyConsumed();
	return remove;
}

uint16_t decodeStatUpdateDp(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_STATUPDATE_DP");
	const uint16_t dp = reader.H(); // SM_STATUPDATE_DP.java:24, writeH(currentDp)
	reader.expectFullyConsumed();
	return dp;
}

DpInfo decodeDpInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DP_INFO");
	DpInfo info;
	info.objectId = reader.D();  // SM_DP_INFO.java:21, writeD(playerObjectId)
	info.currentDp = reader.H(); // :22, writeH(currentDp)
	reader.expectFullyConsumed();
	return info;
}

Resurrect decodeResurrect(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_RESURRECT");
	Resurrect resurrect;
	resurrect.name = reader.S();    // SM_RESURRECT.java:26, writeS(name)
	resurrect.skillId = reader.H(); // :27, writeH(skillId)
	reader.expectD(0, "SM_RESURRECT's last int (writeD(0))"); // :28
	reader.expectFullyConsumed();
	return resurrect;
}

MantraEffect decodeMantraEffect(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_MANTRA_EFFECT");
	reader.expectD(0, "SM_MANTRA_EFFECT's first int (writeD(0x00))"); // SM_MANTRA_EFFECT.java:22
	MantraEffect mantra;
	mantra.effectorObjectId = reader.D(); // :23, writeD(effector.getObjectId())
	mantra.subEffectId = reader.H();      // :24, writeH(subEffectId)
	reader.expectFullyConsumed();
	return mantra;
}

RideRobot decodeRideRobot(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_RIDE_ROBOT");
	RideRobot robot;
	robot.objectId = reader.D(); // SM_RIDE_ROBOT.java:26, writeD(objectId)
	robot.robotId = reader.D();  // :27, writeD(robotId)
	reader.expectFullyConsumed();
	return robot;
}

FlyTime decodeFlyTime(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FLY_TIME");
	FlyTime fly;
	fly.currentFp = reader.D(); // SM_FLY_TIME.java:21, writeD(currentFp)
	fly.maxFp = reader.D();     // :22, writeD(maxFp)
	reader.expectFullyConsumed();
	return fly;
}

Message decodeMessage(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_MESSAGE");
	Message message;
	message.chatType = reader.C();       // SM_MESSAGE.java:139, writeC(chatType.getId())
	message.senderRace = reader.C();     // :140, writeC(isStaff ? 0 : senderRace): 0 all, 1 elyos, 2 asmodian
	if (message.senderRace > 2)
		reader.fail("the race filter is " + std::to_string(message.senderRace) + ", but SM_MESSAGE writes 0, 1 or 2 (SM_MESSAGE.java:42-46)");
	message.senderObjectId = reader.D(); // :141, writeD(senderObjectId)
	message.senderName = reader.S();     // :142, writeS(senderName)
	message.message = reader.S();        // :143, writeS(message)
	if (message.chatType == CHAT_SHOUT) { // :144-148
		message.x = reader.F();
		message.y = reader.F();
		message.z = reader.F();
	}
	reader.expectFullyConsumed();
	return message;
}

SummonPanel decodeSummonPanel(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SUMMON_PANEL");
	SummonPanel panel;
	panel.objectId = reader.D();                            // SM_SUMMON_PANEL.java:21, writeD(summon.getObjectId())
	panel.level = reader.H();                               // :22, writeH(summon.getLevel())
	reader.expectD(0, "SM_SUMMON_PANEL's first unknown int"); // :23, writeD(0)
	reader.expectD(0, "SM_SUMMON_PANEL's second unknown int"); // :24, writeD(0)
	panel.currentHp = reader.D();                           // :25
	panel.maxHp = reader.D();                               // :26
	panel.mainHandPAttack = reader.D();                     // :27
	panel.pDef = reader.D();                                // :28
	panel.mDef = reader.D();                                // :29
	reader.expectH(0, "SM_SUMMON_PANEL's unknown short");    // :30, writeH(0)
	panel.liveTime = reader.D();                            // :31, writeD(summon.getLiveTime())
	reader.expectFullyConsumed();
	return panel;
}

SummonUpdate decodeSummonUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SUMMON_UPDATE");
	SummonUpdate update;
	update.level = reader.C();                                // SM_SUMMON_UPDATE.java:22
	update.mode = reader.H();                                 // :23, writeH(summon.getVisibleMode().getId())
	reader.expectD(0, "SM_SUMMON_UPDATE's first unknown int");  // :24
	reader.expectD(0, "SM_SUMMON_UPDATE's second unknown int"); // :25
	update.currentHp = reader.D();                            // :26
	update.maxHp = reader.D();                                // :29
	update.mainHandPAttack = reader.D();                      // :32
	update.pDef = reader.D();                                 // :35
	update.mResist = reader.H();                              // :38
	update.mDef = reader.D();                                 // :41
	update.accuracy = reader.H();                             // :44
	update.mainHandPCritical = reader.H();                    // :47
	update.mBoost = reader.H();                               // :50
	update.suppression = reader.H();                          // :53
	update.mAccuracy = reader.H();                            // :56
	update.mCritical = reader.H();                            // :59
	update.parry = reader.H();                                // :62
	update.evasion = reader.H();                              // :65
	update.baseMaxHp = reader.D();                            // :67
	update.baseMainHandPAttack = reader.D();                  // :68
	update.basePDef = reader.D();                             // :69
	update.baseMResist = reader.H();                          // :70
	update.baseMDef = reader.D();                             // :71
	update.baseAccuracy = reader.H();                         // :72
	update.baseMainHandPCritical = reader.H();                // :73
	update.baseMBoost = reader.H();                           // :74
	update.baseSuppression = reader.H();                      // :75
	update.baseMAccuracy = reader.H();                        // :76
	update.baseMCritical = reader.H();                        // :77
	update.baseParry = reader.H();                            // :78
	update.baseEvasion = reader.H();                          // :79
	reader.expectFullyConsumed();
	return update;
}

uint16_t decodeSummonPanelRemove(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SUMMON_PANEL_REMOVE");
	const uint16_t skillId = reader.H(); // SM_SUMMON_PANEL_REMOVE.java:20, writeH(skillId)
	const uint8_t flag = reader.C();     // :21-24, writeC(skillId != 0 ? 1 : 0)
	if (flag != (skillId != 0 ? 1 : 0))
		reader.fail("the flag is " + std::to_string(flag) + " for skill " + std::to_string(skillId) +
		            ", but SM_SUMMON_PANEL_REMOVE.java:21-24 writes skillId != 0 ? 1 : 0");
	reader.expectFullyConsumed();
	return skillId;
}

int32_t decodeSummonOwnerRemove(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SUMMON_OWNER_REMOVE");
	const int32_t objectId = reader.D(); // SM_SUMMON_OWNER_REMOVE.java:19, writeD(summonObjId)
	reader.expectFullyConsumed();
	return objectId;
}

} // namespace aion::gameserver::scenario::decoders
