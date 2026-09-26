package com.aionemu.gameserver.dao;

import java.sql.Timestamp;
import java.util.List;
import java.util.Set;

import com.aionemu.gameserver.model.Race;
import com.aionemu.gameserver.model.gameobjects.player.Player;

public class BarDAO {

	public static boolean isNameUsed(String name) {
		return false;
	}

	public static void storePlayer(Player player, Timestamp lastOnline) {
	}

	public static List<Integer> loadIds(byte[] data) {
		return null;
	}

	public static Set<Race> races() {
		return null;
	}
}
