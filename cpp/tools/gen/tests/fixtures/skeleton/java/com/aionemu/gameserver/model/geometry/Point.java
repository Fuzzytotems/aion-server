package com.aionemu.gameserver.model.geometry;

public record Point(float x, float y) implements Comparable<Point> {

	@Override
	public int compareTo(Point o) {
		return Float.compare(x, o.x);
	}
}
