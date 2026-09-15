#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/mail/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author kosyachok, Source, Neon
 */
class SM_MAIL_SERVICE : public AionServerPacket {
public:
	static constexpr int32_t STATIC_BODY_SIZE = 8;
	/**
	 * Java: Function<Letter, Integer> `(letter) -> 22 + byteLengthForString(letter.getSenderName()) + byteLengthForString(letter.getTitle())`
	 * (SplitList part sizes); defined in the .cpp
	 */
	static const runtime::PinnedCallback<int32_t(model::gameobjects::Letter&)> DYNAMIC_BODY_PART_SIZE_CALCULATOR;
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	int32_t serviceId{};
	std::vector<runtime::Ref<model::gameobjects::Letter>> letters{};
	int32_t mailMessage{};
	runtime::Ref<model::gameobjects::Letter> letter{};
	int64_t time{};
	int32_t letterId{};
	std::vector<int32_t> letterIds{};
	int8_t attachmentType{};
	bool isLastPacket{};

public:
	SM_MAIL_SERVICE();
	/** Send mailMessage(ex. Send OK, Mailbox full etc.) */
	explicit SM_MAIL_SERVICE(model::templates::mail::MailMessage mailMessage);
	/** Send mailbox info */
	SM_MAIL_SERVICE(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::Letter>>& letters,
		bool isLastPacket);
	/** used when reading letter */
	SM_MAIL_SERVICE(model::gameobjects::player::Player& player, model::gameobjects::Letter& letter, int64_t time);
	/** used when getting attached items */
	SM_MAIL_SERVICE(int32_t letterId, int8_t attachmentType);
	/** used when deleting letter */
	explicit SM_MAIL_SERVICE(std::span<const int32_t> letterIds);
	~SM_MAIL_SERVICE() override;

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
private:
	void writeLettersList(const std::vector<runtime::Ptr<model::gameobjects::Letter>>& letters);
	void writeMailMessage(int32_t messageId);
	void writeMailboxState(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount);
	void writeLetterRead(model::gameobjects::Letter& letter, int64_t time, int32_t totalCount, int32_t unreadCount, int32_t expressCount,
		int32_t blackCloudCount);
	void writeLetterState(int32_t letterId, int8_t attachmentType);
	void writeLetterDelete(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount,
		std::initializer_list<int32_t> letterIds = {});
	/** C++ only: writeLetterDelete with the stored letter ids (Java passes the int[] field as the varargs array) */
	void writeLetterDelete(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount,
		std::span<const int32_t> letterIds);
};

} // namespace aion::gameserver::network::aion::serverpackets
