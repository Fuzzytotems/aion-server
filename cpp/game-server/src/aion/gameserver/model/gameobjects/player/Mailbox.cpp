#include "aion/gameserver/model/gameobjects/player/Mailbox.h"

#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/services/mail/MailService.h"

namespace aion::gameserver::model::gameobjects::player {

Mailbox::Mailbox(Player& player) : OwnedPart(player), owner(player) {
}

Mailbox::~Mailbox() = default;

void Mailbox::putLetterToMailbox(Letter& letter) {
	if (haveFreeSlots())
		mails.put(letter.getObjectId(), runtime::Ref<Letter>(letter));
	else
		reserveMail.put(letter.getObjectId(), runtime::Ref<Letter>(letter));
}

std::vector<runtime::Ptr<Letter>> Mailbox::getLetters() {
	return mails.values().toVector();
}

std::vector<runtime::Ptr<Letter>> Mailbox::getNewSystemLetters(std::string_view substring) {
	std::vector<runtime::Ptr<Letter>> letters;
	if (substring.starts_with("%") || substring.starts_with("$$")) {
		std::optional<commons::database::Timestamp> lastOnline = owner.getCommonData()->getLastOnline();
		int64_t lastOnlineMillis = !lastOnline ? 0 : lastOnline->time_since_epoch().count();
		for (const runtime::Ptr<Letter>& letter : mails.values()) {
			// Java: letter.getTimeStamp().getTime() (NullPointerException for a letter without a time stamp)
			if (!letter->isUnread() || lastOnlineMillis > letter->getTimeStamp().value().time_since_epoch().count())
				continue;
			// Java: letter.getSenderName() == null || ...; a null sender name is empty here, which never starts with "%" or "$$"
			if (!letter->getSenderName().starts_with(substring))
				continue;
			letters.push_back(letter);
		}
	}
	return letters;
}

runtime::Ptr<Letter> Mailbox::getLetterFromMailbox(int32_t letterObjId) {
	return mails.get(letterObjId);
}

bool Mailbox::haveUnread() {
	for (const runtime::Ptr<Letter>& letter : mails.values()) {
		if (letter->isUnread())
			return true;
	}
	return false;
}

int32_t Mailbox::getUnreadCount() {
	int32_t unreadCount = 0;
	for (const runtime::Ptr<Letter>& letter : mails.values()) {
		if (letter->isUnread())
			unreadCount++;
	}
	return unreadCount;
}

bool Mailbox::haveUnreadByType(LetterType letterType) {
	for (const runtime::Ptr<Letter>& letter : mails.values()) {
		if (letter->isUnread() && letter->getLetterType() == letterType)
			return true;
	}
	return false;
}

int32_t Mailbox::getUnreadCountByType(LetterType letterType) {
	int32_t count = 0;
	for (const runtime::Ptr<Letter>& letter : mails.values()) {
		if (letter->isUnread() && letter->getLetterType() == letterType)
			count++;
	}
	return count;
}

bool Mailbox::haveFreeSlots() {
	return mails.size() < 100;
}

void Mailbox::removeLetter(int32_t letterId) {
	mails.remove(letterId);
	uploadReserveLetters();
	owner.getCommonData()->setMailboxLetters(size());
}

int32_t Mailbox::size() {
	return mails.size() + reserveMail.size();
}

void Mailbox::uploadReserveLetters() {
	if (reserveMail.size() > 0 && haveFreeSlots()) {
		for (const runtime::Ptr<Letter>& letter : reserveMail.values()) {
			if (haveFreeSlots()) {
				mails.put(letter->getObjectId(), runtime::Ref<Letter>(letter));
				reserveMail.remove(letter->getObjectId());
			} else {
				break;
			}
		}
		services::mail::MailService::sendMailList(getOwner(), false, true);
	}
}

} // namespace aion::gameserver::model::gameobjects::player
