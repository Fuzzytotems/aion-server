#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Message [chat, etc]
 *
 * @author -Nemesiss-, Sweetkr, Neon
 */
class SM_MESSAGE : public AionServerPacket {
public:
	static constexpr int32_t MESSAGE_SIZE_HARDCAP = 4000;
	static constexpr int32_t MESSAGE_SIZE_LIMIT = 1022;
private:
	int32_t senderObjectId{};
	std::string message{};
	std::string senderName{};
	int8_t senderRace{};
	model::ChatType chatType{};
	float x{};
	float y{};
	float z{};
public:
	/** Constructs new <tt>SM_MESSAGE</tt> packet. */
	SM_MESSAGE(model::gameobjects::player::Player& sender, std::string_view message, model::ChatType chatType);
	/** Constructs new <tt>SM_MESSAGE</tt> packet. */
	SM_MESSAGE(model::gameobjects::Npc& sender, std::string_view message, model::ChatType chatType);
	/** Manual creation of chat message. A Java null senderName (TheHexwayInstance.java:320) is "": writeS writes the same bytes. */
	SM_MESSAGE(int32_t senderObjectId, std::string_view senderName, std::string_view message, model::ChatType chatType);
private:
	SM_MESSAGE(runtime::Ptr<model::gameobjects::Creature> sender, int32_t senderObjectId, std::string_view senderName, std::string_view message,
		model::ChatType chatType);
public:

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
