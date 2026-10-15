#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //send: sends custom packets to the GM's client, from the mappings ./data/packets/<file>.xml.
 * <p>
 * C++: the handler-local JAXB classes Packets, Packet and Part are records read from the file's DOM (xml::XmlDocument; m5j item 3, owner's
 * decision 2026-10-13), with their JAXB defaults; there is no XSD validation at run time (static-data.md §3.4). A packet is built when its
 * scheduled send runs, from its recorded elements (Java builds it first and schedules the same object).
 *
 * @author Aquanox, Neon
 */
class Send : public AdminCommand {
public:
	Send();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	/** Java: private static class Part (type and value: null when missing; repeat 1) */
	struct Part {
		std::optional<std::string> type;
		std::optional<std::string> value;
		int32_t repeatCount = 1;
	};

	/** Java: private static class Packet (opcode "-1", delay 0) */
	struct Packet {
		std::vector<Part> parts;
		std::string opcode = "-1";
		int64_t delay = 0;
	};

	/** Java: private static class Packets (delay -1; packets null without <packet> elements) */
	struct Packets {
		std::optional<std::vector<Packet>> packets;
		int64_t delay = -1;
	};

	static Packets readPackets(const std::string& file);

	void send(Player& player, const Packets& packets);
};

} // namespace aion::gameserver::handlers::admincommands
