#include "aion/gameserver/handlers/admincommands/Debug.h"

#include <algorithm>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Debug);

// Java Debug.java:24-31
Debug::Debug()
	: AdminCommand("debug", "Helps fixing runtime problems.",
		  "connections - Displays all connected game clients.\n"
		  "connectedPlayers - Displays information about connected players.\n"
		  "dcBuggedPlayers - Disconnects and attempts to save bugged players.\n") {
}

// Java Debug.java:33-74
void Debug::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase; // parity: C++ using-declaration
	if (params.empty()) {
		sendInfo(admin);
		return;
	}
	if (equalsIgnoreCase("connections", params[0])) { // parity= if ("connections".equalsIgnoreCase(params[0])) {
		std::vector<std::shared_ptr<network::aion::AionConnection>> connections = findAionConnections(); // parity= List<AionConnection> connections = findAionConnections();
		std::vector<std::string> texts; // parity= sendInfo(admin, "Online clients:\n\t" + connections.stream().map(AionConnection::toString).collect(Collectors.joining("\n\t")));
		for (const std::shared_ptr<network::aion::AionConnection>& connection : connections) // parity: (the stream above)
			texts.push_back(connection->toString()); // parity: (the same)
		sendInfo(admin, "Online clients:\n\t" + commons::utils::StringUtils::join(texts, "\n\t")); // parity: (the same)
	} else if (equalsIgnoreCase("connectedPlayers", params[0])) { // parity= } else if ("connectedPlayers".equalsIgnoreCase(params[0])) {
		std::vector<runtime::Ptr<Player>> connectedPlayers = findConnectedPlayers(); // parity= List<Player> connectedPlayers = findConnectedPlayers();
		std::string message = "Connected players (" + std::to_string(connectedPlayers.size()) + "):"; // parity= String message = "Connected players (" + connectedPlayers.size() + "):";
		for (const runtime::Ptr<Player>& player : connectedPlayers) { // parity= for (Player player : connectedPlayers) {
			std::string details = player->getPosition()->toCoordString() + ", spawned: " + (player->isSpawned() ? "true" : "false"); // parity= String details = player.getPosition().toCoordString() + ", spawned: " + player.isSpawned();
			if (!player->isInWorld()) {
				details += ", " + utils::ChatUtil::color("not in world", utils::JavaColor::RED); // parity= details += ", " + ChatUtil.color("not in world", Color.RED);
			}
			message += "\n\t" + name(*player) + " - " + utils::ChatUtil::position("Location", *player->getPosition()) + ": " + details; // parity= message += "\n\t" + name(player) + " - " + ChatUtil.position("Location", player.getPosition()) + ": " + details;
		}
		sendInfo(admin, message);
	} else if (equalsIgnoreCase("dcBuggedPlayers", params[0])) { // parity= } else if ("dcBuggedPlayers".equalsIgnoreCase(params[0])) {
		std::vector<runtime::Ptr<Player>> buggedPlayers; // parity= List<Player> buggedPlayers = findConnectedPlayers().stream().filter(p -> !p.isInWorld()).toList();
		for (const runtime::Ptr<Player>& p : findConnectedPlayers()) // parity: (the stream above)
			if (!p->isInWorld()) // parity: (the same)
				buggedPlayers.push_back(p); // parity: (the same)
		if (buggedPlayers.empty()) {
			sendInfo(admin, "No bugged players found.");
		} else {
			for (const runtime::Ptr<Player>& player : buggedPlayers) { // parity= for (Player player : buggedPlayers) {
				player->getController().cancelAllTasks(); // ensure to cancel item update task etc
				player->getCommonData()->setOnline(false);
				services::player::PlayerService::storePlayer(*player); // parity= PlayerService.storePlayer(player);
				player->getClientConnection()->setActivePlayer(nullptr); // parity= player.getClientConnection().setActivePlayer(null);
				player->getClientConnection()->close();
				player->setClientConnection(nullptr); // parity= player.setClientConnection(null);
			}
			std::vector<std::string> texts; // parity= sendInfo(admin, "Saved most data and disconnected the following players:\n" + buggedPlayers);
			for (const runtime::Ptr<Player>& player : buggedPlayers) // parity: (List.toString, above)
				texts.push_back(player->toString()); // parity: (the same)
			sendInfo(admin, "Saved most data and disconnected the following players:\n[" + commons::utils::StringUtils::join(texts, ", ") + "]"); // parity: (the same)
		}
	} else {
		sendInfo(admin);
	}
}

// Java Debug.java:76-79
std::vector<runtime::Ptr<Player>> Debug::findConnectedPlayers() {
	std::vector<std::shared_ptr<network::aion::AionConnection>> connections = findAionConnections(); // parity= List<AionConnection> connections = findAionConnections();
	std::vector<runtime::Ptr<Player>> players; // parity= return connections.stream().map(AionConnection::getActivePlayer).filter(Objects::nonNull).sorted(Comparator.comparing(Player::getName)).toList();
	for (const std::shared_ptr<network::aion::AionConnection>& connection : connections) // parity: (the stream above)
		if (runtime::Ptr<Player> player = connection->getActivePlayer()) // parity: (the same)
			players.push_back(player); // parity: (the same)
	// String.compareTo: UTF-8 byte order is the code point order, which equals Java's UTF-16 order below U+E000 (names are BMP letters)
	std::stable_sort(players.begin(), players.end(), [](const runtime::Ptr<Player>& a, const runtime::Ptr<Player>& b) { return a->getName() < b->getName(); }); // parity: (the same)
	return players; // parity: (the same)
}

// Java Debug.java:81-94 (the catch below is reflection only): the private GameServer.nioServer and its selector keys read through reflection; C++ asks the running client NioServer
// for its connections (handlers-and-porting-plan.md:305) and keeps the AionConnections
std::vector<std::shared_ptr<network::aion::AionConnection>> Debug::findAionConnections() {
	commons::network::NioServer* nioServer = GameServer::getNioServer(); // parity= Field nioServerField = GameServer.class.getDeclaredField("nioServer"); boolean oldAccessible = nioServerField.canAccess(null); nioServerField.setAccessible(true); NioServer nioServer = (NioServer) nioServerField.get(null); nioServerField.setAccessible(oldAccessible);
	if (nioServer == nullptr) // parity: Java's NullPointerException of nioServer.getReadWriteDispatcher() before the server runs, explicit
		throw runtime::NullPointerException("GameServer.nioServer"); // parity: (the same)
	std::vector<std::shared_ptr<network::aion::AionConnection>> connections; // parity= java.util.Set<SelectionKey> keys = nioServer.getReadWriteDispatcher().selector().keys();
	for (const std::shared_ptr<commons::network::AConnectionBase>& o : nioServer->snapshotConnections()) // parity= return keys.stream().map(SelectionKey::attachment).filter(o -> o instanceof AionConnection).map(o -> (AionConnection) o).toList();
		if (std::shared_ptr<network::aion::AionConnection> connection = std::dynamic_pointer_cast<network::aion::AionConnection>(o)) // parity: (the same)
			connections.push_back(connection); // parity: (the same)
	return connections; // parity: (the same)
	// parity= } catch (NoSuchFieldException | SecurityException | IllegalAccessException e) { throw new IllegalArgumentException(e.toString());
}

} // namespace aion::gameserver::handlers::admincommands
