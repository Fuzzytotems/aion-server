package com.aionemu.gameserver.network.aion.serverpackets;

import java.util.List;

import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.aionemu.gameserver.network.aion.AionConnection;
import com.aionemu.gameserver.network.aion.AionServerPacket;

public class SM_ITEMS extends AionServerPacket {

	private final List<Player> players;

	public SM_ITEMS(List<Player> players) {
		super(2);
		this.players = players;
	}

	@Override
	protected void writeImpl(AionConnection con) {
	}

	public static class Page extends SM_ITEMS {

		public Page(List<Player> players, int page) {
			super(players);
		}
	}
}
