#include "aion/gameserver/handlers/admincommands/SysMail.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/LetterTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/services/mail/MailFormatter.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(SysMail);

SysMail::SysMail() : AdminCommand("sysmail") {
}

// parity= enum RecipientType { ELYOS, ASMO, ALL, PLAYER; (SysMail.h)

// Java SysMail.java:35-46
bool SysMail::isAllowed(RecipientType type, Race race) {
	switch (type) {
		case RecipientType::ELYOS:
			return race == Race::ELYOS;
		case RecipientType::ASMO:
			return race == Race::ASMODIANS;
		case RecipientType::ALL:
			return race == Race::ELYOS || race == Race::ASMODIANS;
		default:
			return false;
	}
}

std::string SysMail::recipientTypeName(RecipientType type) { // parity: C++ string conversion of the enum
	switch (type) { // parity: (the same)
		case RecipientType::ELYOS: // parity: (the same)
			return "ELYOS"; // parity: (the same)
		case RecipientType::ASMO: // parity: (the same)
			return "ASMO"; // parity: (the same)
		case RecipientType::ALL: // parity: (the same)
			return "ALL"; // parity: (the same)
		default: // parity: (the same)
			return "PLAYER"; // parity: (the same)
	} // parity: (the same)
} // parity: (the same)

// Java SysMail.java:49-158
void SysMail::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 5) { // parity= if (params.length < 5) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	std::vector<std::string> paramValues(params.begin(), params.end()); // parity= String[] paramValues = new String[params.length]; System.arraycopy(params, 0, paramValues, 0, params.length);

	std::optional<RecipientType> recipientType; // parity= RecipientType recipientType = null;
	std::string sender; // parity= String sender = null;
	std::optional<std::string> recipient; // parity= String recipient = null;

	if (paramValues[0].starts_with("$$") || paramValues[0].starts_with("%")) { // parity= if (paramValues[0].startsWith("$$") || paramValues[0].startsWith("%")) {
		if (params.size() < 6) { // parity= if (params.length < 6) {
			info(admin, std::nullopt); // parity= info(admin, null);
			return;
		}
		sender = paramValues[0];
		paramValues.assign(params.begin() + 1, params.end()); // parity= paramValues = new String[params.length - 1]; System.arraycopy(params, 1, paramValues, 0, params.length - 1);
	} else {
		sender = "Admin";
	}

	if (paramValues[0].starts_with("@")) { // parity= if (paramValues[0].startsWith("@")) {
		if (std::string_view("@all").starts_with(paramValues[0])) // parity= if ("@all".startsWith(paramValues[0]))
			recipientType = RecipientType::ALL;
		else if (std::string_view("@elyos").starts_with(paramValues[0])) // parity= else if ("@elyos".startsWith(paramValues[0]))
			recipientType = RecipientType::ELYOS;
		else if (std::string_view("@asmodians").starts_with(paramValues[0])) // parity= else if ("@asmodians".startsWith(paramValues[0]))
			recipientType = RecipientType::ASMO;
		else {
			PacketSendUtility::sendMessage(admin, "Recipient must be Player name, @all, @elyos or @asmodians.");
			return;
		}
	} else {
		recipientType = RecipientType::PLAYER;
		recipient = Util::convertName(paramValues[0]);
	}

	int32_t item = 0, count = 0, kinah = 0;
	LetterType letterType;

	try {
		item = commons::utils::parseInt(paramValues[2]); // parity= item = Integer.parseInt(paramValues[2]);
		count = commons::utils::parseInt(paramValues[3]); // parity= count = Integer.parseInt(paramValues[3]);
		kinah = commons::utils::parseInt(paramValues[4]); // parity= kinah = Integer.parseInt(paramValues[4]);
		letterType = model::gameobjects::getLetterTypeById(commons::utils::parseInt(paramValues[1])); // parity= letterType = LetterType.getLetterTypeById(Integer.parseInt(paramValues[1]));
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(admin, "<Regular|Blackcloud|Express> <Item|Count|Kinah> value must be an integer.");
		return;
	}

	if (letterType == LetterType::BLACKCLOUD)
		sender = "$$CASH_ITEM_MAIL";

	bool express = checkExpress(admin, item, count, kinah, recipient, recipientType, letterType);
	if (!express)
		return;

	if (item <= 0)
		item = 0;

	if (count <= 0)
		count = -1;

	std::string title = "System Mail";
	std::string message = " ";

	if (paramValues.size() > 5) { // parity= if (paramValues.length > 5) {
		std::vector<std::string> words(paramValues.begin() + 5, paramValues.end()); // parity= String[] words = new String[paramValues.length - 5]; System.arraycopy(paramValues, 5, words, 0, words.length);
		std::vector<std::string> outText(1); // parity= String[] outText = new String[1];
		int32_t wordCount = extractText(words, outText);
		if (wordCount > 0) {
			title = outText[0];
			std::vector<std::string> msgWords(words.begin() + wordCount, words.end()); // parity= String[] msgWords = new String[words.length - wordCount]; System.arraycopy(words, wordCount, msgWords, 0, msgWords.length);
			wordCount = extractText(msgWords, outText);
			if (wordCount > 0)
				message = outText[0];
		}
	}

	if (recipientType == RecipientType::PLAYER) {
		if (letterType == LetterType::BLACKCLOUD)
			services::mail::MailFormatter::sendBlackCloudMail(*recipient, item, count); // parity= MailFormatter.sendBlackCloudMail(recipient, item, count);
		else
			services::mail::SystemMailService::sendMail(sender, *recipient, title, message, item, count, kinah, letterType); // parity= SystemMailService.sendMail(sender, recipient, title, message, item, count, kinah, letterType);
	} else {
		for (const runtime::Ptr<Player>& player : World::getInstance().getAllPlayers()) { // parity= for (Player player : World.getInstance().getAllPlayers()) {
			if (isAllowed(*recipientType, player->getRace())) { // parity= if (recipientType.isAllowed(player.getRace())) {
				if (letterType == LetterType::BLACKCLOUD)
					services::mail::MailFormatter::sendBlackCloudMail(player->getName(), item, count); // parity= MailFormatter.sendBlackCloudMail(player.getName(), item, count);
				else
					services::mail::SystemMailService::sendMail(sender, player->getName(), title, message, item, count, kinah, letterType); // parity= SystemMailService.sendMail(sender, player.getName(), title, message, item, count, kinah, letterType);
			}
		}
	}

	if (item != 0) {
		PacketSendUtility::sendMessage(admin, "You send to " + recipientTypeName(*recipientType) + // parity= PacketSendUtility.sendMessage(admin, "You send to " + recipientType + (recipientType == RecipientType.PLAYER ? " " + recipient : "") + "\n" + "[item:" + item + "] Count:" + count + " Kinah:" + kinah + "\n" + "Letter send successfully.");
			(recipientType == RecipientType::PLAYER ? " " + *recipient : "") + "\n" + "[item:" + std::to_string(item) + "] Count:" + // parity: (the same)
			std::to_string(count) + " Kinah:" + std::to_string(kinah) + "\n" + "Letter send successfully."); // parity: (the same)
	} else if (kinah > 0) {
		PacketSendUtility::sendMessage(admin, "You send to " + recipientTypeName(*recipientType) + // parity= PacketSendUtility.sendMessage(admin, "You send to " + recipientType + (recipientType == RecipientType.PLAYER ? " " + recipient : "") + "\n" + " Kinah:" + kinah + "\n" + "Letter send successfully.");
			(recipientType == RecipientType::PLAYER ? " " + *recipient : "") + "\n" + " Kinah:" + std::to_string(kinah) + "\n" + // parity: (the same)
			"Letter send successfully."); // parity: (the same)
	}
}

// Java SysMail.java:160-190
int32_t SysMail::extractText(const std::vector<std::string>& words, std::vector<std::string>& outText) {
	if (words.size() == 0 || outText.size() == 0) // parity= if (words.length == 0 || outText.length == 0)
		return 0;

	if (!words[0].starts_with("|")) // parity= if (!words[0].startsWith("|"))
		return 0;

	int32_t wordCount = 1;

	std::string enclosedText = words[0].substr(1); // parity= String enclosedText = words[0].substring(1);
	if (enclosedText.ends_with("|")) { // parity= if (enclosedText.endsWith("|")) {
		outText[0] = enclosedText.substr(0, enclosedText.size() - 1); // parity= outText[0] = enclosedText.substring(0, enclosedText.length() - 1);
	} else {
		std::vector<std::string> titleWords; // parity= List<String> titleWords = new ArrayList<>();
		titleWords.push_back(enclosedText); // parity= titleWords.add(enclosedText);
		for (; wordCount < static_cast<int32_t>(words.size()); wordCount++) { // parity= for (; wordCount < words.length; wordCount++) {
			std::string word = words[wordCount];
			if (word.ends_with("|")) { // parity= if (word.endsWith("|")) {
				word = word.substr(0, word.size() - 1); // parity= word = word.substring(0, word.length() - 1);
				titleWords.push_back(word); // parity= titleWords.add(word);
				wordCount++;
				break;
			} else
				titleWords.push_back(word); // parity= titleWords.add(word);
		}

		outText[0] = join(titleWords, 0); // parity= outText[0] = String.join(" ", titleWords);
	}

	return wordCount;
}

// Java SysMail.java:192-255
bool SysMail::checkExpress(Player& admin, int32_t item, int32_t count, int32_t kinah, const std::optional<std::string>& recipient,
	std::optional<RecipientType> recipientType, LetterType letterType) {
	bool shouldExpress = false;

	if (!recipientType) { // parity= if (recipientType == null) {
		PacketSendUtility::sendMessage(admin, "Please insert Recipient Type.\n" "Recipient = player, @all, @elyos or @asmodians"); // parity= PacketSendUtility.sendMessage(admin, "Please insert Recipient Type.\n" + "Recipient = player, @all, @elyos or @asmodians");
		return false;
	} else if (recipientType == RecipientType::PLAYER) {
		if (letterType == LetterType::NORMAL) {
			if (!dao::PlayerDAO::isNameUsed(*recipient)) { // parity= if (!PlayerDAO.isNameUsed(recipient)) {
				PacketSendUtility::sendMessage(admin, "Could not find a Recipient by that name.");
				return false;
			}
			shouldExpress = true;
		} else if (letterType == LetterType::EXPRESS) {
			if (World::getInstance().getPlayer(*recipient) == nullptr) { // parity= if (World.getInstance().getPlayer(recipient) == null) {
				PacketSendUtility::sendMessage(admin, "This Recipient is offline.");
				return false;
			}
			shouldExpress = true;
		} else { // Black cloud
			shouldExpress = World::getInstance().getPlayer(*recipient) != nullptr; // parity= shouldExpress = World.getInstance().getPlayer(recipient) != null;
		}
	} else {
		shouldExpress = letterType != LetterType::NORMAL;
	}

	if (item == 0 && count != 0) {
		PacketSendUtility::sendMessage(admin, "Please insert Item Id..");
		return false;
	}

	if (count == 0 && item != 0) {
		PacketSendUtility::sendMessage(admin, "Please insert Item Count.");
		return false;
	}

	if (count <= 0 && item <= 0 && kinah <= 0) {
		PacketSendUtility::sendMessage(admin, "Parameters <Item> <Count> <Kinah> are icorrect.");
		return false;
	}

	const model::templates::item::ItemTemplate* itemTemplate = DataManager::ITEM_DATA->getItemTemplate(item); // parity= ItemTemplate itemTemplate = DataManager.ITEM_DATA.getItemTemplate(item);
	if (item != 0) {
		if (itemTemplate == nullptr) {
			PacketSendUtility::sendMessage(admin, "Item id is incorrect: " + std::to_string(item)); // parity= PacketSendUtility.sendMessage(admin, "Item id is incorrect: " + item);
			return false;
		}
		int64_t maxStackCount = itemTemplate->getMaxStackCount();
		if (count > maxStackCount && maxStackCount != 0) {
			PacketSendUtility::sendMessage(admin, "Please insert correct Item Count.");
			return false;
		}
	}

	if (kinah < 0) {
		PacketSendUtility::sendMessage(admin, "Kinah value must be >= 0.");
		return false;
	} else if (kinah > 0 && letterType == LetterType::BLACKCLOUD) {
		PacketSendUtility::sendMessage(admin, "Kinah attachment are not for black cloud letters!");
		return false;
	}
	return shouldExpress;
}

// Java SysMail.java:257-265
void SysMail::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, // parity= PacketSendUtility.sendMessage(player, "No parameters detected.\n" + "Please use //sysmail [%|$$<Sender>] <Recipient> <Regular|Blackcloud|Express> <Item> <Count> <Kinah> [|Title|] [|Message|]\n" + "Sender name must start with % or $$. Can be ommitted.\n" + "Regular mail type is 0, Express mail type is 1, Blackcloud type is 2.\n" + "If parameters (Item, Count) = 0 than the item will not be send\n" + "If parameters (Kinah) = 0 not send Kinah\n" + "Recipient = Player name, @all, @elyos or @asmodians\n" + "Optional Title and Message must be enclosed within pipe chars");
		"No parameters detected.\n" // parity: (the same)
		"Please use //sysmail [%|$$<Sender>] <Recipient> <Regular|Blackcloud|Express> <Item> <Count> <Kinah> [|Title|] [|Message|]\n" // parity: (the same)
		"Sender name must start with % or $$. Can be ommitted.\n" // parity: (the same)
		"Regular mail type is 0, Express mail type is 1, Blackcloud type is 2.\n" // parity: (the same)
		"If parameters (Item, Count) = 0 than the item will not be send\n" // parity: (the same)
		"If parameters (Kinah) = 0 not send Kinah\n" // parity: (the same)
		"Recipient = Player name, @all, @elyos or @asmodians\n" // parity: (the same)
		"Optional Title and Message must be enclosed within pipe chars"); // parity: (the same)
}

} // namespace aion::gameserver::handlers::admincommands
