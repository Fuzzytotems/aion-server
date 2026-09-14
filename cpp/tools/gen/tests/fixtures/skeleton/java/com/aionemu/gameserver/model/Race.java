package com.aionemu.gameserver.model;

/**
 * Races.
 */
public enum Race {
	ELYOS,
	ASMODIANS,
	NPC;

	public boolean isPlayerRace() {
		return this != NPC;
	}

	public enum Group {
		PLAYERS, OTHERS
	}
}
