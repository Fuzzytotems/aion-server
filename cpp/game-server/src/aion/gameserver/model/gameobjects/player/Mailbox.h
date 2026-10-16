#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`PartSlot<Mailbox>`, set with setMailbox), bound to the player in
 * the constructor.
 *
 * @author kosyachok, Atracer
 */
class Mailbox : public runtime::OwnedPart {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<Letter>> mails{AION_LOCK_CLASS(Mailbox::mails#stripe)};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<Letter>> reserveMail{AION_LOCK_CLASS(Mailbox::reserveMail#stripe)};
	runtime::OwnerRef<Player> owner;

public:
	// 0x00 - closed
	// 0x01 - regular
	// 0x02 - express
	runtime::Field<int8_t> mailBoxState{0};

	explicit Mailbox(Player& player);

	~Mailbox() override;

	void putLetterToMailbox(Letter& letter);

	/** Get all letters in mailbox (sorted according to time received) */
	std::vector<runtime::Ptr<Letter>> getLetters();

	/** Get system letters by sender name substring which were received after the last login */
	std::vector<runtime::Ptr<Letter>> getNewSystemLetters(std::string_view substring);

	/** Get letter with specified letter id */
	runtime::Ptr<Letter> getLetterFromMailbox(int32_t letterObjId);

	/** Check whether mailbox contains unread letters */
	bool haveUnread();

	/** Java final */
	int32_t getUnreadCount();

	bool haveUnreadByType(LetterType letterType);

	/** Java final */
	int32_t getUnreadCountByType(LetterType letterType);

	bool haveFreeSlots();

	void removeLetter(int32_t letterId);

	/** Current size of mailbox */
	int32_t size();

	void uploadReserveLetters();

	Player& getOwner() const { return owner; }
};

} // namespace aion::gameserver::model::gameobjects::player
