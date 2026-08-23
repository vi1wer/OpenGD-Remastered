/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License    
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*************************************************************************/

#include "GroupCommandObject.h"
#include "EffectGameObject.h"
#include <algorithm>
#include <cmath>

namespace
{
float easeT(float t, int ease, float rate)
{
	t = std::clamp(t, 0.f, 1.f);
	const float r = rate > 0.01f ? rate : 2.f;
	switch (ease)
	{
	case 1: // EaseInOut
		return t < 0.5f ? 0.5f * std::pow(t * 2.f, r) : 1.f - 0.5f * std::pow((1.f - t) * 2.f, r);
	case 2: // EaseIn
		return std::pow(t, r);
	case 3: // EaseOut
		return 1.f - std::pow(1.f - t, r);
	case 13: // SineInOut
		return 0.5f - 0.5f * std::cos(t * 3.14159265f);
	case 14: // SineIn
		return 1.f - std::cos(t * 1.57079633f);
	case 15: // SineOut
		return std::sin(t * 1.57079633f);
	case 10: // ExpoInOut
		if (t <= 0.f || t >= 1.f)
			return t;
		return t < 0.5f ? 0.5f * std::pow(2.f, 20.f * t - 10.f) : 1.f - 0.5f * std::pow(2.f, -20.f * t + 10.f);
	case 11: // ExpoIn
		return t <= 0.f ? 0.f : std::pow(2.f, 10.f * t - 10.f);
	case 12: // ExpoOut
		return t >= 1.f ? 1.f : 1.f - std::pow(2.f, -10.f * t);
	default:
		return t;
	}
}
} // namespace

GroupCommandObject* GroupCommandObject::create()
{
	GroupCommandObject* ret = new GroupCommandObject();
	if (ret->init())
	{
		ret->autorelease();
		return ret;
	}
	else
	{
		delete ret;
		ret = nullptr;
		return nullptr;
	}
}

void GroupCommandObject::runMoveCommand(float duration, ax::Point offsetPos, int easeType, float easeAmt)
{
	_finalPoint = offsetPos;
	_duration = duration;
	_elapsed = 0.f;
	_easeType = easeType;
	_easeAmt = easeAmt;
	_actionDone = false;
	_actionDoneForNextLoop = false;
	_newPos = ax::Point::ZERO;
	_oldPos = ax::Point::ZERO;
	_currentOffset = ax::Point::ZERO;
	_unkPoint = ax::Point::ZERO;
	_action1 = nullptr;
	_action2 = nullptr;

	// Instant move unless this command also follows the player.
	if (duration <= 0.f && !_followPlayerX && !_followPlayerY)
	{
		_newPos = offsetPos;
		_currentOffset = offsetPos;
		_oldPos = offsetPos;
		_unkPoint = offsetPos;
		_actionDone = true;
		return;
	}

	if (duration <= 0.f)
		_duration = 0.0001f;
}

void GroupCommandObject::updateTweenAction(float value, std::string_view key)
{
	if (key == "2") // y
	{
		const float delta = value - _currentOffset.y;
		_unkPoint.y += value - _oldPos.y;
		_oldPos.y = value;
		_currentOffset.y = value;
		_newPos.y = delta;
	}
	else if (key == "1") // x
	{
		const float delta = value - _currentOffset.x;
		_unkPoint.x += value - _oldPos.x;
		_oldPos.x = value;
		_currentOffset.x = value;
		_newPos.x = delta;
	}
}

void GroupCommandObject::step(float dt)
{
	_elapsed += dt;
	if (_actionDone)
		return;

	if (_action1)
	{
		_action1->step(dt);
		if (_action2)
			_action2->step(dt);
		if (_action1->isDone())
		{
			_action1->stop();
			_action1->release();
			_action1 = nullptr;
			if (_action2)
			{
				_action2->stop();
				_action2->release();
				_action2 = nullptr;
			}
			_actionDone = true;
		}
		return;
	}

	const float t = _duration <= 0.f ? 1.f : std::min(1.f, _elapsed / _duration);
	const float eased = easeT(t, _easeType, _easeAmt);
	const ax::Point target{_finalPoint.x * eased, _finalPoint.y * eased};
	_newPos = target - _currentOffset;
	_unkPoint = _newPos;
	_oldPos = target;
	_currentOffset = target;

	if (t >= 1.f && !_followPlayerX && !_followPlayerY)
		_actionDone = true;
	else if (t >= 1.f && _elapsed >= _duration)
		_actionDone = true;
}
