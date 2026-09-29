#include "decoders/QuestDecoders.h"

#include <algorithm>
#include <set>
#include <string>

namespace aion::gameserver::scenario::decoders {

// ---- SM_QUEST_ACTION --------------------------------------------------------------------------------------------------------------------

namespace {

/** writeC(status) of ADD and UPDATE: QuestStatus.value(), 3-6 (QuestStatus.java:11-14) */
uint8_t readStatus(BodyReader& reader) {
	const uint8_t status = reader.C();
	if (status < QUEST_STATUS_START || status > QUEST_STATUS_LOCKED)
		reader.fail("the status byte is " + std::to_string(status) + ", but QuestStatus.value() is 3 (START) to 6 (LOCKED)");
	return status;
}

} // namespace

QuestAction decodeQuestAction(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_QUEST_ACTION");
	QuestAction action;
	if (body.empty()) {
		// SM_QUEST_ACTION.java:69-71: a quest template with an extra_category other than NONE returns before the first write
		action.empty = true;
		return action;
	}
	action.actionType = reader.C(); // :72, actionType.getId()
	action.questId = reader.D();    // :73
	switch (action.actionType) {
		case QUEST_ACTION_ADD:
			action.status = readStatus(reader);                      // :76
			reader.expectC(0, "the byte after ADD's status");         // :77, writeC(0x0)
			action.questVarsAndFlags = reader.D();                    // :78, writeD(step | flags << 24)
			reader.expectH(0, "ADD's short after the quest vars");    // :79, writeH(0)
			reader.expectC(0, "ADD's last byte");                     // :80, writeC(0), "seen sometimes 1 for campaign quests"
			break;
		case QUEST_ACTION_UPDATE:
			action.status = readStatus(reader);                        // :83
			reader.expectC(0, "the byte after UPDATE's status");        // :84, writeC(0x0)
			action.questVarsAndFlags = reader.D();                      // :85, writeD(step | flags << 24)
			reader.expectH(0, "UPDATE's short after the quest vars");   // :86, writeH(0), "seen sometimes 1 when status == COMPLETED"
			break;
		case QUEST_ACTION_ABANDON:
			reader.expectD(0, "ABANDON's int"); // :89, writeD(0)
			break;
		case QUEST_ACTION_TIMER: {
			action.timer = reader.D();          // :92
			const uint8_t running = reader.C(); // :93, writeC(timer > 0 ? 1 : 0)
			if (running != (action.timer > 0 ? 1 : 0))
				reader.fail("TIMER's byte is " + std::to_string(running) + " for a timer of " + std::to_string(action.timer) +
				            ", but SM_QUEST_ACTION.java:93 writes timer > 0 ? 1 : 0");
			break;
		}
		case QUEST_ACTION_SHARE: {
			action.sharerId = reader.D();        // :96
			const int32_t alliance = reader.D(); // :97, writeD(shareInAlliance ? 1 : 0)
			if (alliance != 0 && alliance != 1)
				reader.fail("SHARE's alliance int is " + std::to_string(alliance) + ", but SM_QUEST_ACTION.java:97 writes 0 or 1");
			action.shareInAlliance = alliance == 1;
			break;
		}
		case QUEST_ACTION_UNK:
			reader.expectH(1, "UNK's first short");  // :100, writeH(0x01)
			reader.expectH(0, "UNK's second short"); // :101, writeH(0x0)
			break;
		default:
			reader.fail("action type " + std::to_string(action.actionType) + " is none of SM_QUEST_ACTION.ActionType's 1-6");
	}
	reader.expectFullyConsumed();
	return action;
}

// ---- SM_NEARBY_QUESTS -------------------------------------------------------------------------------------------------------------------

std::vector<int32_t> NearbyQuests::ids() const {
	std::vector<int32_t> result;
	result.reserve(quests.size());
	for (const NearbyQuest& quest : quests)
		result.push_back(quest.questId);
	return result;
}

std::vector<int32_t> NearbyQuests::notYetAvailableIds() const {
	std::vector<int32_t> result;
	for (const NearbyQuest& quest : quests)
		if (quest.notYetAvailable)
			result.push_back(quest.questId);
	return result;
}

std::vector<int32_t> NearbyQuests::wireValues() const {
	std::vector<int32_t> result;
	result.reserve(quests.size());
	for (const NearbyQuest& quest : quests)
		result.push_back(quest.wire);
	return result;
}

bool NearbyQuests::contains(int32_t questId) const {
	return std::ranges::any_of(quests, [questId](const NearbyQuest& quest) { return quest.questId == questId; });
}

NearbyQuests decodeNearbyQuests(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_NEARBY_QUESTS");
	NearbyQuests nearby;
	reader.expectC(0, "the leading byte"); // SM_NEARBY_QUESTS.java:23, writeC(0)
	// :24, writeH(-nearbyQuestList.size() & 0xFFFF): the count is written negated, like SM_QUEST_LIST's
	const uint16_t negated = reader.H();
	const size_t count = static_cast<size_t>((0x10000u - negated) & 0xFFFFu);
	std::set<int32_t> seen;
	for (size_t i = 0; i < count; i++) {
		NearbyQuest quest;
		quest.wire = reader.D(); // :25-29, questId | notYetAvailableBit for a grey quest
		if (quest.wire < 0 || quest.wire >= (NEARBY_QUEST_NOT_YET_AVAILABLE_BIT << 1))
			reader.fail("entry " + std::to_string(i) + " is " + std::to_string(quest.wire) +
			            ": a quest id (at most 99002, quest_data.xml) with at most the marker bit 17 set is below 2^18");
		quest.notYetAvailable = (quest.wire & NEARBY_QUEST_NOT_YET_AVAILABLE_BIT) != 0;
		quest.questId = quest.wire & ~NEARBY_QUEST_NOT_YET_AVAILABLE_BIT;
		if (!seen.insert(quest.questId).second)
			reader.fail("quest " + std::to_string(quest.questId) + " is written twice, but the entries are the keys of a Map");
		nearby.quests.push_back(quest);
	}
	reader.expectFullyConsumed();
	return nearby;
}

// ---- SM_STATUPDATE_EXP ------------------------------------------------------------------------------------------------------------------

StatUpdateExp decodeStatUpdateExp(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_STATUPDATE_EXP");
	StatUpdateExp exp;
	exp.currentExp = reader.Q();     // SM_STATUPDATE_EXP.java:36
	exp.recoverableExp = reader.Q(); // :37
	exp.maxExp = reader.Q();         // :38
	exp.curBoostExp = reader.Q();    // :39
	exp.maxBoostExp = reader.Q();    // :40
	reader.expectFullyConsumed();
	return exp;
}

} // namespace aion::gameserver::scenario::decoders
