#include "GameSession.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "decoders/SkillDecoders.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::scenario {

using network::test::PacketWriter;

void CharacterAppearance::writeTo(PacketWriter& writer) const {
	writer.D(voice).D(skinRGB).D(hairRGB).D(eyeRGB).D(lipRGB);
	writer.C(face).C(hair).C(deco).C(tattoo).C(faceContour).C(expression).C(unknown4).C(jawLine).C(forehead);
	writer.C(eyeHeight).C(eyeSpace).C(eyeWidth).C(eyeSize).C(eyeShape).C(eyeAngle);
	writer.C(browHeight).C(browAngle).C(browShape);
	writer.C(nose).C(noseBridge).C(noseWidth).C(noseTip);
	writer.C(cheek).C(lipHeight).C(mouthSize).C(lipSize).C(smile).C(lipShape).C(jawHeight).C(chinJut).C(earShape).C(headSize);
	writer.C(neck).C(neckLength);
	writer.C(shoulderSize);
	writer.C(torso).C(chest).C(waist).C(hips);
	writer.C(armThickness);
	writer.C(handSize).C(legThickness);
	writer.C(footSize).C(facialRate);
	writer.C(unknown0).C(armLength).C(legLength).C(shoulders).C(faceShape);
	writer.C(unknownA).C(unknownB).C(unknownC);
	writer.F(height);
}

GameSession::GameSession(uint16_t port) : client(port) {
}

std::string GameSession::nameOf(int32_t opcode) {
	if (const auto* entry = network::aion::ServerPacketsOpcodes::findByOpcode(opcode))
		return std::string(entry->name);
	return "SM_UNKNOWN_" + std::to_string(opcode);
}

GameSession::Packet GameSession::record(const network::test::FakeGameClient::ServerPacket& serverPacket) {
	Packet packet;
	packet.opcode = serverPacket.opcode;
	packet.name = nameOf(serverPacket.opcode);
	packet.data = serverPacket.data;
	packet.receivedAt = std::chrono::steady_clock::now();
	packets.push_back(packet);
	return packet;
}

int32_t GameSession::readKey(std::chrono::milliseconds timeout) {
	int32_t key = client.readKey(timeout);
	network::test::FakeGameClient::ServerPacket packet;
	packet.opcode = network::test::FakeGameClientCrypto::SM_KEY_OPCODE;
	packet.data = PacketWriter().D(key).data;
	record(packet);
	return key;
}

void GameSession::send(int32_t opcode, std::span<const uint8_t> data) {
	client.sendPacket(opcode, data);
}

std::optional<GameSession::Packet> GameSession::next(std::chrono::milliseconds timeout) {
	std::optional<network::test::FakeGameClient::ServerPacket> packet = client.readPacket(timeout);
	if (!packet)
		return std::nullopt;
	return record(*packet);
}

GameSession::Packet GameSession::expect(std::string_view name, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			throw std::runtime_error("timeout waiting for " + std::string(name));
		std::optional<Packet> packet = next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet)
			throw std::runtime_error("no " + std::string(name) + " (" + (client.socket.isClosed() ? "connection closed" : "timeout") + ")");
		if (packet->name == name)
			return *packet;
	}
}

std::vector<GameSession::Packet> GameSession::collectUntilQuiet(std::chrono::milliseconds quiet, std::chrono::milliseconds limit) {
	std::vector<Packet> collected;
	const auto deadline = std::chrono::steady_clock::now() + limit;
	while (std::chrono::steady_clock::now() < deadline) {
		std::optional<Packet> packet = next(quiet);
		if (!packet)
			break;
		collected.push_back(std::move(*packet));
	}
	return collected;
}

std::vector<std::string> GameSession::names(size_t from) const {
	std::vector<std::string> result;
	for (size_t i = from; i < packets.size(); i++)
		result.push_back(packets[i].name);
	return result;
}

bool GameSession::waitClosed(std::chrono::milliseconds timeout) {
	return client.socket.waitClosed(timeout);
}

std::vector<uint8_t> GameSession::buildCM_VERSION_CHECK(uint16_t clientVersion) {
	// readImpl: readUH aionClientVersion, readUH npcScriptInterfaceVersion, readD windowsEncoding, readD windowsVersion, readD windowsSubVersion,
	// readC liteInfo
	return PacketWriter().H(clientVersion).H(0).D(1252).D(10).D(0).C(2).data;
}

std::vector<uint8_t> GameSession::buildCM_L2AUTH_LOGIN_CHECK(int32_t playOk2, int32_t playOk1, int32_t accountId, int32_t loginOk) {
	// readImpl: playOk2, playOk1, accountId, loginOk, unk1, unk2
	return PacketWriter().D(playOk2).D(playOk1).D(accountId).D(loginOk).D(0).D(0).data;
}

std::vector<uint8_t> GameSession::buildCM_MAC_ADDRESS(std::string_view macAddress, std::string_view hddSerial) {
	// readImpl: readC unk, readUH routeSteps, routeSteps x readD, readS macAddress, readS hddSerial, readD local IP
	return PacketWriter().C(0).H(1).D(0x0100007F).S(macAddress).S(hddSerial).D(0x0100007F).data;
}

std::vector<uint8_t> GameSession::buildCM_TIME_CHECK(int32_t nanoTime) {
	return PacketWriter().D(nanoTime).data;
}

std::vector<uint8_t> GameSession::buildCM_CHARACTER_LIST(int32_t playOk2) {
	return PacketWriter().D(playOk2).data;
}

std::vector<uint8_t> GameSession::buildCM_PING() {
	return PacketWriter().H(0).data; // readH unk
}

std::vector<uint8_t> GameSession::buildCM_GAMEGUARD(std::span<const uint8_t> data) {
	return PacketWriter().D(static_cast<int32_t>(data.size())).B(data).data; // readD size, readB(size)
}

std::vector<uint8_t> GameSession::buildCM_SECURITY_TOKEN() {
	return {};
}

std::vector<uint8_t> GameSession::buildCM_CHECK_NICKNAME(std::string_view nick) {
	return PacketWriter().S(nick).data;
}

std::vector<uint8_t> GameSession::buildCM_CREATE_CHARACTER(int32_t accountId, std::string_view accountName, const NewCharacter& character,
	uint8_t type) {
	PacketWriter writer;
	writer.D(accountId).S(accountName);
	// readBasicInfo: readS(25) name (the string, then (25 - length) * 2 padding bytes), gender, race, player class
	writer.S(character.name);
	const int32_t length = static_cast<int32_t>(commons::utils::StringUtils::toUtf16(character.name).size());
	if (length < 25)
		writer.zeros(static_cast<size_t>((25 - length) * 2));
	writer.D(character.female ? 1 : 0).D(character.asmodian ? 1 : 0).D(character.playerClassId);
	character.appearance.writeTo(writer);
	writer.C(type); // readUC type
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_MAY_LOGIN_INTO_GAME() {
	return {};
}

std::vector<uint8_t> GameSession::buildCM_ENTER_WORLD(int32_t objectId) {
	return PacketWriter().D(objectId).data;
}

std::vector<uint8_t> GameSession::buildCM_LEVEL_READY() {
	return {};
}

std::vector<uint8_t> GameSession::buildCM_MOVE(float x, float y, float z, int8_t heading, int8_t type, float x2, float y2, float z2) {
	// readImpl: x, y, z, heading, type; POSITION|MANUAL: ABSOLUTE -> x2, y2, z2, otherwise the vector (the scenario sends ABSOLUTE)
	return PacketWriter().F(x).F(y).F(z).C(heading).C(type).F(x2).F(y2).F(z2).data;
}

std::vector<uint8_t> GameSession::buildCM_MOVE(float x, float y, float z, int8_t heading, int8_t type) {
	return PacketWriter().F(x).F(y).F(z).C(heading).C(type).data;
}

std::vector<uint8_t> GameSession::buildCM_QUIT(bool stayConnected) {
	return PacketWriter().C(stayConnected ? 1 : 0).data;
}

std::vector<uint8_t> GameSession::buildCM_CUSTOM_SETTINGS(uint16_t display, uint16_t deny) {
	return PacketWriter().H(display).H(deny).data;
}

std::vector<uint8_t> GameSession::buildCM_SUBZONE_CHANGE(uint8_t unk) {
	return PacketWriter().C(unk).data;
}

std::vector<uint8_t> GameSession::buildCM_TARGET_SELECT(int32_t targetObjectId, bool selectTargetOfTarget) {
	return PacketWriter().D(targetObjectId).C(selectTargetOfTarget ? 1 : 0).data;
}

std::vector<uint8_t> GameSession::buildCM_ATTACK(int32_t targetObjectId, uint8_t attackNo, uint16_t time, uint8_t type) {
	return PacketWriter().D(targetObjectId).C(attackNo).H(time).C(type).data;
}

std::vector<uint8_t> GameSession::buildCM_REVIVE(uint8_t reviveId) {
	return PacketWriter().C(reviveId).data;
}

std::vector<uint8_t> GameSession::buildCM_CASTSPELL(const CastRequest& request) {
	// readImpl: readUH spellid, readUC level, readUC targetType, the arm of CM_CASTSPELL.java:43-67, readUH hitTime, readD unk
	PacketWriter writer;
	writer.H(request.spellId).C(request.level).C(request.targetType);
	switch (request.targetType) {
		case 0:
		case 3:
		case 4:
			writer.D(request.targetObjectId); // :47
			break;
		case 1:
			writer.F(request.x).F(request.y).F(request.z); // :50-52
			break;
		case 2:
			writer.F(request.x).F(request.y).F(request.z); // :55-57
			for (int i = 0; i < 8; i++)
				writer.F(0.0f); // :58-65, "unk1" .. "unk8"
			break;
		default:
			break; // no arm: the switch has no default
	}
	writer.H(request.hitTime).D(request.unk); // :69-70
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_CASTSPELL(uint16_t spellId, uint8_t level, uint8_t targetType, int32_t targetObjectId, uint16_t hitTime) {
	if (targetType == 1 || targetType == 2)
		throw std::invalid_argument("CM_CASTSPELL target type " + std::to_string(targetType) + " reads a point, not an object id");
	CastRequest request;
	request.spellId = spellId;
	request.level = level;
	request.targetType = targetType;
	request.targetObjectId = targetObjectId;
	request.hitTime = hitTime;
	return buildCM_CASTSPELL(request);
}

std::vector<uint8_t> GameSession::buildCM_REMOVE_ALTERED_STATE(uint16_t skillId, uint8_t unk1, uint8_t unk2) {
	return PacketWriter().H(skillId).C(unk1).C(unk2).data;
}

std::vector<uint8_t> GameSession::buildCM_START_LOOT(int32_t targetObjectId, uint8_t action) {
	return PacketWriter().D(targetObjectId).C(action).data; // CM_START_LOOT.java:36-37
}

std::vector<uint8_t> GameSession::buildCM_LOOT_ITEM(int32_t targetObjectId, uint8_t index) {
	return PacketWriter().D(targetObjectId).C(index).data; // CM_LOOT_ITEM.java:24-25
}

std::vector<uint8_t> GameSession::buildCM_USE_ITEM(int32_t uniqueItemId, int8_t type, int32_t extra) {
	PacketWriter writer;
	writer.D(uniqueItemId).C(type); // CM_USE_ITEM.java:39-40
	if (type == 2 || type == 5 || type == 6)
		writer.D(extra); // :42-50: targetItemId, syncId or indexReturn
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_MOVE_ITEM(int32_t itemObjId, uint8_t source, uint8_t destination, int16_t slot) {
	return PacketWriter().D(itemObjId).C(source).C(destination).H(slot).data; // CM_MOVE_ITEM.java:26-29
}

std::vector<uint8_t> GameSession::buildCM_SPLIT_ITEM(int32_t sourceItemObjId, int64_t itemAmount, uint8_t sourceStorageType, int32_t destinationItemObjId,
	uint8_t destinationStorageType, int16_t slotNum) {
	// CM_SPLIT_ITEM.java:28-33
	return PacketWriter().D(sourceItemObjId).Q(itemAmount).C(sourceStorageType).D(destinationItemObjId).C(destinationStorageType).H(slotNum).data;
}

std::vector<uint8_t> GameSession::buildCM_REPLACE_ITEM(uint8_t sourceStorageType, int32_t sourceItemObjId, uint8_t replaceStorageType,
	int32_t replaceItemObjId) {
	return PacketWriter().C(sourceStorageType).D(sourceItemObjId).C(replaceStorageType).D(replaceItemObjId).data; // CM_REPLACE_ITEM.java:26-29
}

std::vector<uint8_t> GameSession::buildCM_MANASTONE(const ManastoneRequest& request) {
	PacketWriter writer;
	writer.C(request.actionType).C(request.targetFusedSlot).D(request.targetItemUniqueId); // CM_MANASTONE.java:40-42
	switch (request.actionType) {
		case 1:
		case 2:
		case 4:
		case 8:
			writer.D(request.stoneUniqueId).D(request.supplementUniqueId); // :48-49
			break;
		case MANASTONE_REMOVE:
			writer.C(request.slotNum).C(0).H(0).D(request.npcObjId); // :52-55, the readC and readH are dropped
			break;
		default:
			break; // no arm: the switch has no default
	}
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_MANASTONE(uint8_t actionType, uint8_t targetFusedSlot, int32_t targetItemUniqueId, int32_t stoneUniqueId,
	int32_t supplementUniqueId) {
	if (actionType == MANASTONE_REMOVE)
		throw std::invalid_argument("CM_MANASTONE action 3 reads a slot and an npc, not two item ids");
	ManastoneRequest request;
	request.actionType = actionType;
	request.targetFusedSlot = targetFusedSlot;
	request.targetItemUniqueId = targetItemUniqueId;
	request.stoneUniqueId = stoneUniqueId;
	request.supplementUniqueId = supplementUniqueId;
	return buildCM_MANASTONE(request);
}

std::vector<uint8_t> GameSession::buildCM_EQUIP_ITEM(uint8_t action, int64_t slot, int32_t itemObjId) {
	return PacketWriter().C(action).Q(slot).D(itemObjId).data; // CM_EQUIP_ITEM.java:30-32
}

std::vector<uint8_t> GameSession::buildCM_DELETE_ITEM(int32_t itemObjectId) {
	return PacketWriter().D(itemObjectId).data; // CM_DELETE_ITEM.java:27
}

std::vector<uint8_t> GameSession::buildCM_SHOW_DIALOG(int32_t targetObjectId) {
	return PacketWriter().D(targetObjectId).data; // CM_SHOW_DIALOG.java:24
}

std::vector<uint8_t> GameSession::buildCM_CLOSE_DIALOG(int32_t targetObjectId) {
	return PacketWriter().D(targetObjectId).data; // CM_CLOSE_DIALOG.java:25
}

std::vector<uint8_t> GameSession::buildCM_DIALOG_SELECT(int32_t targetObjectId, uint16_t dialogActionId, uint16_t extendedRewardIndex,
	uint16_t lastPage, int32_t questId, uint16_t unk) {
	// CM_DIALOG_SELECT.java:48-53
	return PacketWriter().D(targetObjectId).H(dialogActionId).H(extendedRewardIndex).H(lastPage).D(questId).H(unk).data;
}

std::vector<uint8_t> GameSession::buildCM_QUESTION_RESPONSE(int32_t questionId, uint8_t response, int32_t senderId) {
	// CM_QUESTION_RESPONSE.java:28-35: the readC, readH, readD and readH the server drops are written as 0
	return PacketWriter().D(questionId).C(response).C(0).H(0).D(senderId).D(0).H(0).data;
}

std::vector<uint8_t> GameSession::buildCM_DELETE_QUEST(int32_t questId) {
	return PacketWriter().D(questId).data; // CM_DELETE_QUEST.java:24
}

std::vector<uint8_t> GameSession::buildCM_PLAY_MOVIE_END(uint8_t type, int32_t targetObjectId, int32_t questId, int32_t movieId) {
	return PacketWriter().C(type).D(targetObjectId).D(questId).D(movieId).C(0).C(0).data; // CM_PLAY_MOVIE_END.java:34-39
}

std::vector<uint8_t> GameSession::buildCM_INVITE_TO_GROUP(uint8_t inviteType, std::string_view playerName) {
	return PacketWriter().C(inviteType).S(playerName).data; // CM_INVITE_TO_GROUP.java:31-32
}

std::vector<uint8_t> GameSession::buildCM_PLAYER_STATUS_INFO(uint8_t commandCode, int32_t selectedObjectId, int32_t allianceGroupId,
	int32_t secondObjectId) {
	return PacketWriter().C(commandCode).D(selectedObjectId).D(allianceGroupId).D(secondObjectId).data; // CM_PLAYER_STATUS_INFO.java:32-35
}

std::vector<uint8_t> GameSession::buildCM_DISTRIBUTION_SETTINGS(int32_t lootRule, int32_t misc, std::array<int32_t, 6> qualityWords) {
	PacketWriter writer;
	writer.D(0).D(lootRule).D(misc); // CM_DISTRIBUTION_SETTINGS.java:42-50: isLeague, lootRule, misc
	for (int32_t word : qualityWords)
		writer.D(word); // :51-56
	return writer.D(0).data; // :57 unk
}

std::vector<uint8_t> GameSession::buildCM_GROUP_DISTRIBUTION(int64_t amount, uint8_t partyType) {
	return PacketWriter().Q(amount).C(partyType).data; // CM_GROUP_DISTRIBUTION.java:30-31
}

std::vector<uint8_t> GameSession::buildCM_SHOW_BRAND(int32_t action, int32_t brandId, int32_t targetObjectId) {
	return PacketWriter().D(action).D(brandId).D(targetObjectId).data; // CM_SHOW_BRAND.java:32-34
}

std::vector<uint8_t> GameSession::buildCM_GROUP_DATA_EXCHANGE(uint8_t action, uint8_t groupType, uint8_t unk2, std::span<const uint8_t> data) {
	PacketWriter writer;
	writer.C(action); // CM_GROUP_DATA_EXCHANGE.java:37
	if (action != 1)
		writer.C(groupType).C(unk2); // :38-41
	return writer.D(static_cast<int32_t>(data.size())).B(data).data; // :42-43
}

std::vector<uint8_t> GameSession::buildCM_FIND_GROUP_LIST() {
	return PacketWriter().C(0).data; // CM_FIND_GROUP.java:40-43
}

std::vector<uint8_t> GameSession::buildCM_FIND_GROUP_OFFER(int32_t playerOrTeamId, std::string_view message, uint8_t groupType) {
	return PacketWriter().C(2).D(playerOrTeamId).S(message).C(groupType).data; // CM_FIND_GROUP.java:53-57
}

std::vector<uint8_t> GameSession::buildCM_GROUP_LOOT(int32_t groupId, int32_t index, int32_t itemId, int32_t npcObjId, uint8_t distributionMode,
	int32_t roll, int64_t bid) {
	// CM_GROUP_LOOT.java:45-55: groupId, index, unk1, itemId, unk2, unk3, unk4, npcObjId, distributionMode, roll, bid
	return PacketWriter().D(groupId).D(index).D(0).D(itemId).C(0).C(0).C(0).D(npcObjId).C(distributionMode).D(roll).Q(bid).data;
}

std::vector<uint8_t> GameSession::buildCM_QUEST_SHARE(int32_t questId) {
	return PacketWriter().D(questId).data; // CM_QUEST_SHARE.java:40
}

namespace {

/** the readUH count every M5c list packet starts with: more than 65535 entries cannot be written */
uint16_t listCount(size_t size, std::string_view packet) {
	if (size > 0xFFFF)
		throw std::invalid_argument(std::string(packet) + ": " + std::to_string(size) + " entries do not fit its readUH count");
	return static_cast<uint16_t>(size);
}

} // namespace

std::vector<uint8_t> GameSession::buildCM_BUY_ITEM(int32_t sellerObjectId, int16_t tradeActionId, std::span<const BuyItemEntry> entries) {
	PacketWriter writer;
	writer.D(sellerObjectId).H(tradeActionId).H(listCount(entries.size(), "CM_BUY_ITEM")); // CM_BUY_ITEM.java:49-51
	for (const BuyItemEntry& entry : entries)
		writer.D(entry.itemId).Q(entry.count); // :65-66
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_EXCHANGE_REQUEST(int32_t targetObjectId) {
	return PacketWriter().D(targetObjectId).data; // CM_EXCHANGE_REQUEST.java:35
}

std::vector<uint8_t> GameSession::buildCM_EXCHANGE_ADD_ITEM(int32_t itemObjectId, int32_t itemCount) {
	return PacketWriter().D(itemObjectId).D(itemCount).data; // CM_EXCHANGE_ADD_ITEM.java:24-25
}

std::vector<uint8_t> GameSession::buildCM_EXCHANGE_ADD_KINAH(int64_t kinahCount) {
	return PacketWriter().Q(kinahCount).data; // CM_EXCHANGE_ADD_KINAH.java:22
}

std::vector<uint8_t> GameSession::buildCM_EXCHANGE_LOCK() {
	return {}; // CM_EXCHANGE_LOCK.readImpl reads nothing
}

std::vector<uint8_t> GameSession::buildCM_EXCHANGE_OK() {
	return {}; // CM_EXCHANGE_OK.readImpl reads nothing
}

std::vector<uint8_t> GameSession::buildCM_EXCHANGE_CANCEL() {
	return {}; // CM_EXCHANGE_CANCEL.readImpl reads nothing
}

std::vector<uint8_t> GameSession::buildCM_PRIVATE_STORE(std::span<const PrivateStoreItem> items) {
	PacketWriter writer;
	writer.H(listCount(items.size(), "CM_PRIVATE_STORE")); // CM_PRIVATE_STORE.java:24
	for (const PrivateStoreItem& item : items)
		writer.D(item.itemObjectId).D(item.itemId).H(item.count).Q(item.price); // :27-30
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_PRIVATE_STORE_NAME(std::string_view name) {
	return PacketWriter().S(name).data; // CM_PRIVATE_STORE_NAME.java:28
}

std::vector<uint8_t> GameSession::buildCM_SEND_MAIL(std::string_view recipientName, std::string_view title, std::string_view message,
	int32_t itemObjectId, int64_t itemCount, int64_t kinahCount, uint8_t letterType) {
	// CM_SEND_MAIL.java:30-36
	return PacketWriter().S(recipientName).S(title).S(message).D(itemObjectId).Q(itemCount).Q(kinahCount).C(letterType).data;
}

std::vector<uint8_t> GameSession::buildCM_CHECK_MAIL_LIST(bool expressOnly) {
	return PacketWriter().C(expressOnly ? 1 : 0).data; // CM_CHECK_MAIL_LIST.java:23, `readC() == 1`
}

std::vector<uint8_t> GameSession::buildCM_READ_MAIL(int32_t letterObjectId) {
	return PacketWriter().D(letterObjectId).data; // CM_READ_MAIL.java:23
}

std::vector<uint8_t> GameSession::buildCM_GET_MAIL_ATTACHMENT(int32_t letterObjectId, uint8_t attachmentType) {
	return PacketWriter().D(letterObjectId).C(attachmentType).data; // CM_GET_MAIL_ATTACHMENT.java:24-25
}

std::vector<uint8_t> GameSession::buildCM_DELETE_MAIL(std::span<const int32_t> letterObjectIds) {
	PacketWriter writer;
	writer.H(listCount(letterObjectIds.size(), "CM_DELETE_MAIL")); // CM_DELETE_MAIL.java:23
	for (const int32_t id : letterObjectIds)
		writer.D(id).C(0); // :25-26, the dropped readC written as 0
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_TUNE(int32_t itemObjectId, int32_t tuningScrollObjectId) {
	return PacketWriter().D(itemObjectId).D(tuningScrollObjectId).data; // CM_TUNE.java:26-27
}

std::vector<uint8_t> GameSession::buildCM_TUNE_RESULT(int32_t itemObjectId, bool accepted) {
	return PacketWriter().D(itemObjectId).C(accepted ? 1 : 0).data; // CM_TUNE_RESULT.java:29-30, `readC() == 1`
}

std::vector<uint8_t> GameSession::buildCM_SELECT_DECOMPOSABLE(int32_t objectId, int32_t unk, uint8_t index) {
	return PacketWriter().D(objectId).D(unk).C(index).data; // CM_SELECT_DECOMPOSABLE.java:39-41
}

std::vector<uint8_t> GameSession::buildCM_CRAFT(uint8_t unk, int32_t targetTemplateId, int32_t recipeId, int32_t targetObjectId,
	std::span<const CraftMaterial> materials, uint8_t craftType) {
	PacketWriter writer;
	writer.C(unk).D(targetTemplateId).D(recipeId).D(targetObjectId);           // CM_CRAFT.java:33-36
	writer.H(listCount(materials.size(), "CM_CRAFT")).C(craftType);           // :37-38, the count before the craft type
	for (const CraftMaterial& material : materials)
		writer.D(material.itemId).Q(material.count); // :39-40
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_RECIPE_DELETE(int32_t recipeId) {
	return PacketWriter().D(recipeId).data; // CM_RECIPE_DELETE.java:22
}

std::vector<uint8_t> GameSession::buildCM_MOVE_GLIDE(float x, float y, float z, int8_t heading, int8_t extraType, uint8_t glideFlag) {
	// MovementMask POSITION 0x80, MANUAL 0x40, VEHICLE 0x10 (MovementMask.java:26-41): the POSITION|MANUAL pair would add a target point,
	// VEHICLE five more fields (CM_MOVE.java:48-73); GlideFlag.GEYSER 0x80 (GlideFlag.java:12) adds readUC geyserLocationId (:64-65)
	constexpr uint8_t POSITION_MANUAL = 0xC0, VEHICLE = 0x10;
	const uint8_t type = static_cast<uint8_t>(MOVE_GLIDE | extraType);
	if ((type & POSITION_MANUAL) == POSITION_MANUAL || (type & VEHICLE) != 0)
		throw std::invalid_argument("buildCM_MOVE_GLIDE: type " + std::to_string(type) + " reads more fields than this builder writes");
	if (glideFlag == 0x80)
		throw std::invalid_argument("buildCM_MOVE_GLIDE: GlideFlag.GEYSER reads a windstream location id");
	return PacketWriter().F(x).F(y).F(z).C(heading).C(type).C(glideFlag).data; // CM_MOVE.java:41-46, 63
}

std::vector<uint8_t> GameSession::buildCM_TELEPORT_SELECT(int32_t targetObjectId, int32_t locId) {
	return PacketWriter().D(targetObjectId).D(locId).H(0).data; // CM_TELEPORT_SELECT.java:39-42
}

std::vector<uint8_t> GameSession::buildCM_TELEPORT_ANIMATION_DONE() {
	return {}; // CM_TELEPORT_ANIMATION_DONE.java: readImpl reads nothing
}

std::vector<uint8_t> GameSession::buildCM_INSTANCE_LEAVE() {
	return {}; // CM_INSTANCE_LEAVE.java: "nothing to read"
}

std::vector<uint8_t> GameSession::buildCM_MOVE_IN_AIR(int32_t worldId, float x, float y, float z, int8_t heading, int32_t distance) {
	return PacketWriter().D(worldId).F(x).F(y).F(z).C(heading).D(distance).data; // CM_MOVE_IN_AIR.java:35-41
}

std::vector<uint8_t> GameSession::buildCM_EMOTION(uint8_t emotionType) {
	return PacketWriter().C(emotionType).data; // CM_EMOTION.java:53, readUC; the arms the gate sends read nothing more
}

std::vector<uint8_t> GameSession::buildCM_BIND_POINT_TELEPORT(uint8_t action, int32_t locId, int64_t kinah) {
	PacketWriter writer;
	writer.C(action); // CM_BIND_POINT_TELEPORT.java:26, readC
	if (action == BIND_POINT_TELEPORT_CAST)
		writer.D(locId).Q(kinah); // :28-29, readD locId, readQ kinah
	return writer.data;
}

std::vector<uint8_t> GameSession::buildCM_TOGGLE_SKILL_DEACTIVATE(uint16_t skillId) {
	return PacketWriter().H(skillId).H(0).H(0).data; // CM_TOGGLE_SKILL_DEACTIVATE.java:25-27
}

std::vector<uint8_t> GameSession::buildCM_USE_CHARGE_SKILL() {
	return {}; // CM_USE_CHARGE_SKILL.java:20-21: readImpl reads nothing
}

std::vector<uint8_t> GameSession::buildCM_SUMMON_COMMAND(uint8_t mode, int32_t targetObjectId) {
	return PacketWriter().C(mode).D(0).D(0).D(targetObjectId).data; // CM_SUMMON_COMMAND.java:27-30
}

std::vector<uint8_t> GameSession::buildCM_SUMMON_ATTACK(int32_t summonObjectId, int32_t targetObjectId, uint16_t time) {
	return PacketWriter().D(summonObjectId).D(targetObjectId).C(0).H(time).C(0).data; // CM_SUMMON_ATTACK.java:31-35
}

GameSession::CastOutcome GameSession::castAndWait(int32_t casterObjectId, const CastRequest& request, std::chrono::milliseconds timeout,
	const std::optional<CastInterruption>& interruption) {
	CastOutcome outcome;
	outcome.firstPacket = packets.size();
	send(CM_CASTSPELL, buildCM_CASTSPELL(request));
	outcome.sentAt = std::chrono::steady_clock::now();
	const auto deadline = outcome.sentAt + timeout;
	for (;;) {
		auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		auto until = deadline;
		if (interruption && !outcome.interruptionSentAt) {
			const auto due = outcome.sentAt + interruption->after;
			if (now >= due) {
				send(interruption->opcode, interruption->body);
				outcome.interruptionSentAt = std::chrono::steady_clock::now();
				continue;
			}
			until = std::min(until, due);
		}
		std::optional<Packet> packet = next(std::chrono::ceil<std::chrono::milliseconds>(until - now));
		if (!packet) {
			if (client.socket.isClosed()) { // readPacket answers nothing for a timeout and for a closed connection alike
				outcome.closed = true;
				break;
			}
			continue;
		}
		const size_t index = packets.size() - 1;
		if (packet->name == "SM_CASTSPELL") {
			const decoders::CastSpell cast = decoders::decodeCastSpell(packet->data);
			if (!outcome.castSpell && cast.effectorObjectId == casterObjectId && cast.spellId == request.spellId)
				outcome.castSpell = index;
		} else if (packet->name == "SM_CASTSPELL_RESULT") {
			const decoders::CastSpellResult result = decoders::decodeCastSpellResult(packet->data);
			if (result.effectorObjectId == casterObjectId && result.skillId == request.spellId) {
				outcome.castSpellResult = index;
				break;
			}
		} else if (packet->name == "SM_SKILL_CANCEL") {
			const decoders::SkillCancel cancel = decoders::decodeSkillCancel(packet->data);
			if (cancel.creatureObjectId == casterObjectId && cancel.skillId == request.spellId) {
				outcome.skillCancel = index;
				break;
			}
		}
	}
	outcome.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - outcome.sentAt);
	return outcome;
}

GameSession::FightOutcome GameSession::fightUntil(int32_t targetObjectId, std::chrono::milliseconds attackSpeed, const FightPredicate& done,
	std::chrono::milliseconds timeout, int32_t maxAttacks, uint8_t attackType) {
	FightOutcome outcome;
	outcome.firstPacket = packets.size();
	const auto start = std::chrono::steady_clock::now();
	const auto deadline = start + timeout;
	auto nextAttack = start; // the first attack goes out at once
	for (;;) {
		auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		if (now >= nextAttack) {
			if (outcome.attacksSent >= maxAttacks)
				break; // the last attack has had its interval to be answered in
			send(CM_ATTACK, buildCM_ATTACK(targetObjectId, static_cast<uint8_t>(outcome.attacksSent), 0, attackType));
			outcome.attacksSent++;
			now = std::chrono::steady_clock::now();
			nextAttack = now + attackSpeed;
		}
		const auto until = nextAttack < deadline ? nextAttack : deadline;
		if (now >= until)
			continue;
		std::optional<Packet> packet = next(std::chrono::duration_cast<std::chrono::milliseconds>(until - now));
		if (!packet) {
			if (client.socket.isClosed()) { // readPacket answers nothing for a timeout and for a closed connection alike
				outcome.closed = true;
				break;
			}
			continue; // nothing arrived before the next attack was due
		}
		if (done(*packet)) {
			outcome.done = true;
			break;
		}
	}
	outcome.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	return outcome;
}

GameSession::TalkOutcome GameSession::talk(int32_t npcObjectId, uint16_t dialogActionId, int32_t questId, std::chrono::milliseconds quiet,
	std::chrono::milliseconds limit) {
	TalkOutcome outcome;
	outcome.firstPacket = packets.size();
	send(CM_DIALOG_SELECT, buildCM_DIALOG_SELECT(npcObjectId, dialogActionId, 0, 0, questId));
	outcome.packets = collectUntilQuiet(quiet, limit);
	outcome.closed = client.socket.isClosed();
	return outcome;
}

} // namespace aion::gameserver::scenario
