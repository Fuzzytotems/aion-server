#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model {

/**
 * Companion of the generated enum ChatType (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions found by
 * ADL (`getId(chatType)` for Java `chatType.getId()`). Java's static HashMap id -> ChatType is a lookup over the constexpr table.
 */

namespace detail {
/** Java constructor arguments (id, sysMsg) in ordinal order */
struct ChatTypeData {
	int8_t id;
	bool sysMsg;
};

inline constexpr std::array<ChatTypeData, 29> CHAT_TYPE_DATA{{
	{0, false},  // NORMAL [MT_SAY] Normal chat (White)
	{1, false},  // NPC [MT_THINK] Npc chat (Light Blue)
	{3, false},  // SHOUT [MT_SHOUT] Shout chat (Orange)
	{4, false},  // WHISPER [MT_WHISPER] Whisper chat (Green)
	{5, false},  // GROUP [MT_PARTY] Group chat (Blue)
	{6, false},  // ALLIANCE [MT_ALLIANCE] Alliance chat (Aqua)
	{7, false},  // GROUP_LEADER [MT_ALERT] Group Leader chat (Orange)
	{8, false},  // LEAGUE [MT_UNION] League chat (Dark Blue)
	{9, false},  // LEAGUE_ALERT [MT_UNIONALERT] League chat (Orange)
	{10, false}, // LEGION [MT_GUILD] Legion chat (Green)
	{14, false}, // CH1 [MT_CHANNEL_0]
	{15, false}, // CH2 [MT_CHANNEL_1]
	{16, false}, // CH3 [MT_CHANNEL_2]
	{17, false}, // CH4 [MT_CHANNEL_3]
	{18, false}, // CH5 [MT_CHANNEL_4]
	{19, false}, // CH6 [MT_CHANNEL_5]
	{20, false}, // CH7 [MT_CHANNEL_6]
	{21, false}, // CH8 [MT_CHANNEL_7]
	{22, false}, // CH9 [MT_CHANNEL_8]
	{23, false}, // CH10 [MT_CHANNEL_9]
	{24, false}, // COMMAND [MT_RANKER_CHAT] Command chat (Yellow), usable by commanders and supreme commanders via /c
	{25, true},  // GOLDEN_YELLOW [MT_SYSMSG_HIGH_PRI] System message (Dark Yellow), most commonly used, no "center" equivalent.
	{27, false}, // GM_CHAT [MT_SYSMSG_PETITION] Message used in petition/support packet, has its own window and icon next to skill bar
	{31, true},  // WHITE [MT_GMMSG_NORMAL_LEVEL_1] System message (White), visible in "All" chat thumbnail only !
	{32, true},  // YELLOW [MT_GMMSG_NORMAL_LEVEL_2] System message (Yellow), visible in "All" chat thumbnail only !
	{33, true},  // BRIGHT_YELLOW [MT_GMMSG_NORMAL_LEVEL_3] System message (Light Yellow), visible in "All" chat thumbnail only !
	{34, true},  // WHITE_CENTER [MT_GMMSG_HIGH_LEVEL_1] Periodic Notice (White && Box on screen center)
	{35, true},  // YELLOW_CENTER [MT_GMMSG_HIGH_LEVEL_2] Periodic Announcement (Yellow && Box on screen center)
	{36, true},  // BRIGHT_YELLOW_CENTER [MT_GMMSG_HIGH_LEVEL_3] System Notice (Light Yellow && Box on screen center)
}};
static_assert(static_cast<size_t>(ChatType::BRIGHT_YELLOW_CENTER) + 1 == CHAT_TYPE_DATA.size(), "one entry per ChatType constant");
} // namespace detail

/** Java: ChatType.getId() - chat type in client */
constexpr int8_t getId(ChatType type) noexcept {
	return detail::CHAT_TYPE_DATA[static_cast<size_t>(type)].id;
}

/** Java: ChatType.isSysMsg() - true if this is a system message chat type (all races can read chat) */
constexpr bool isSysMsg(ChatType type) noexcept {
	return detail::CHAT_TYPE_DATA[static_cast<size_t>(type)].sysMsg;
}

/**
 * Java: ChatType.getChatType(byte id)
 *
 * @throws IllegalArgumentException
 *           if can't find suitable chat type
 */
inline ChatType getChatType(int8_t id) {
	for (size_t i = 0; i < detail::CHAT_TYPE_DATA.size(); ++i) {
		if (detail::CHAT_TYPE_DATA[i].id == id)
			return static_cast<ChatType>(i);
	}
	throw runtime::IllegalArgumentException("Unsupported chat type: " + std::to_string(id & 0xFF));
}

} // namespace aion::gameserver::model
