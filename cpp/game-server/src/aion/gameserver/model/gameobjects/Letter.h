#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author kosyachok
 */
class Letter : public AionObject, public Persistable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t recipientId;
	runtime::Field<runtime::Ref<Item>> attachedItem{};
	runtime::Field<int64_t> attachedKinahCount{};
	const std::string senderName;
	const std::string title;
	const std::string message;
	runtime::Field<bool> unread{};
	runtime::Field<bool> express{};
	const commons::database::Timestamp timeStamp;
	runtime::Field<Persistable::PersistentState> persistentState{};
	runtime::Field<LetterType> letterType{};

protected:
	Letter(int32_t objId, int32_t recipientId, runtime::Ptr<Item> attachedItem, int64_t attachedKinahCount, std::string_view title,
		std::string_view message, std::string_view senderName, std::optional<commons::database::Timestamp> timeStamp, bool unread, LetterType letterType);

public:
	static runtime::Ref<Letter> create(int32_t objId, int32_t value, runtime::Ptr<Item> attachedItemValue, int64_t attachedKinahCountValue,
		std::string_view titleValue, std::string_view messageValue, std::string_view senderNameValue,
		std::optional<commons::database::Timestamp> timeStampValue, bool unreadValue, LetterType letterTypeValue);

	std::string getName() override { return this->title; }

	int32_t getRecipientId() const { return this->recipientId; }

	runtime::Ptr<Item> getAttachedItem() const { return this->attachedItem.get(); }

	void setAttachedItem(runtime::Ptr<Item> attachedItem);

	int64_t getAttachedKinah() const { return this->attachedKinahCount.get(); }

	std::string getTitle() const { return this->title; }

	std::string getMessage() const { return this->message; }

	std::string getSenderName() const { return this->senderName; }

	LetterType getLetterType() const { return this->letterType.get(); }

	bool isUnread() const { return this->unread.get(); }

	void setReadLetter();

	bool isExpress() const { return this->express.get(); }

	void setExpress(bool express);

	void setLetterType(LetterType letterType);

	Persistable::PersistentState getPersistentState() override { return this->persistentState.get(); }

	void removeAttachedKinah();

	void setPersistentState(Persistable::PersistentState state) override;

	std::optional<commons::database::Timestamp> getTimeStamp() const { return this->timeStamp; }

protected:
	~Letter() override;
};

} // namespace aion::gameserver::model::gameobjects
