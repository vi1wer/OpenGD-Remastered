/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "2d/DrawNode.h"
#include "math/Vec2.h"
#include <deque>

// Wave path ribbon (Geometry Dash HardStreak).
class HardStreak : public ax::DrawNode
{
public:
	static HardStreak* create();

	void addPoint(const ax::Vec2& pos);
	void reset();
	void resumeStroke();
	void stopStroke();
	void updateStroke();
	void setStroke(float stroke);
	void setTint(const ax::Color3B& color);

private:
	bool init() override;

	std::deque<ax::Vec2> _points;
	ax::Color3B _tint{255, 255, 255};
	float _stroke = 14.f;
	bool _appendPoints = true;
};
