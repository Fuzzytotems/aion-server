#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace aion::chatserver::model {

/**
 * The channel types and the identifiers the client uses for them in channel requests (e.g. "trade" in "@&lt;U+0001&gt;trade_Housing_barrack
 * &lt;U+0001&gt;1.0.AION.KOR").
 * <p>
 * Java: com.aionemu.chatserver.model.ChannelType
 *
 * @author ATracer, Neon
 */
enum class ChannelType : int8_t {
	REGION,
	TRADE,
	LFG,
	JOB,
	LANG
};

/** Java: getIdentifier() */
constexpr std::string_view getIdentifier(ChannelType channelType) noexcept {
	switch (channelType) {
		case ChannelType::REGION:
			return "public";
		case ChannelType::TRADE:
			return "trade";
		case ChannelType::LFG:
			return "partyFind";
		case ChannelType::JOB:
			return "job";
		case ChannelType::LANG:
			return "User";
	}
	return {};
}

/** Java: ChannelType.getByIdentifier(identifier) - the type with this identifier (case sensitive), null (std::nullopt) if there is none */
constexpr std::optional<ChannelType> getByIdentifier(std::string_view identifier) noexcept {
	for (ChannelType ct : {ChannelType::REGION, ChannelType::TRADE, ChannelType::LFG, ChannelType::JOB, ChannelType::LANG}) {
		if (getIdentifier(ct) == identifier)
			return ct;
	}
	return std::nullopt;
}

/** Java: name() */
constexpr std::string_view name(ChannelType channelType) noexcept {
	switch (channelType) {
		case ChannelType::REGION:
			return "REGION";
		case ChannelType::TRADE:
			return "TRADE";
		case ChannelType::LFG:
			return "LFG";
		case ChannelType::JOB:
			return "JOB";
		case ChannelType::LANG:
			return "LANG";
	}
	return {};
}

} // namespace aion::chatserver::model
