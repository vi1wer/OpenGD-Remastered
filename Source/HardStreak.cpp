/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "HardStreak.h"
#include <algorithm>
#include <cmath>
#include <vector>

USING_NS_AX;

namespace
{
constexpr size_t kMaxPoints = 120;
constexpr float kMinDistSq = 20.f;
} // namespace

HardStreak* HardStreak::create()
{
	auto* ret = new (std::nothrow) HardStreak();
	if (ret && ret->init())
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

bool HardStreak::init()
{
	if (!DrawNode::init())
		return false;
	setAnchorPoint(Vec2::ZERO);
	setPosition(Vec2::ZERO);
	return true;
}

void HardStreak::addPoint(const Vec2& pos)
{
	if (!_appendPoints)
		return;
	if (!_points.empty() && _points.back().distanceSquared(pos) < kMinDistSq)
		return;
	_points.push_back(pos);
	while (_points.size() > kMaxPoints)
		_points.pop_front();
}

void HardStreak::reset()
{
	_points.clear();
	clear();
}

void HardStreak::resumeStroke()
{
	_appendPoints = true;
}

void HardStreak::stopStroke()
{
	_appendPoints = false;
}

void HardStreak::setStroke(float stroke)
{
	_stroke = std::max(1.f, stroke);
}

void HardStreak::setTint(const Color3B& color)
{
	_tint = color;
}

void HardStreak::updateStroke()
{
	clear();
	const int n = static_cast<int>(_points.size());
	if (n < 2)
		return;

	std::vector<Vec2> left(n);
	std::vector<Vec2> right(n);
	const float half = _stroke * 0.5f;

	for (int i = 0; i < n; ++i)
	{
		Vec2 dir;
		if (i == n - 1)
			dir = _points[i] - _points[i - 1];
		else if (i == 0)
			dir = _points[i + 1] - _points[i];
		else
			dir = _points[i + 1] - _points[i - 1];

		if (dir.lengthSquared() < 0.0001f)
			dir = Vec2(1.f, 0.f);
		dir.normalize();
		const Vec2 nrm(-dir.y, dir.x);
		left[i] = _points[i] + nrm * half;
		right[i] = _points[i] - nrm * half;
	}

	for (int i = 0; i < n - 1; ++i)
	{
		const float t = static_cast<float>(i + 1) / static_cast<float>(n);
		const uint8_t a = static_cast<uint8_t>(std::clamp(40.f + t * 215.f, 0.f, 255.f));
		const Color4B col(_tint.r, _tint.g, _tint.b, a);
		drawTriangle(left[i], right[i], left[i + 1], col);
		drawTriangle(right[i], right[i + 1], left[i + 1], col);
	}
}
