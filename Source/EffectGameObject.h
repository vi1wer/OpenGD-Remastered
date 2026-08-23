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

#pragma once
#include <string_view>

#include "GameObject.h"
#include "Types.h"
#include "math/Vec2.h"

class BaseGameLayer;
namespace ax 
{ 
	class Action;
	class ActionInterval;
}

class EffectGameObject : public GameObject
{
private:
	BaseGameLayer* _bgl;

public:
	float _duration = 0.f;
	ax::Color3B _color;
	GDHSV _hsv;
	float _opacity = 1.0f;
	int _targetColorId = 1, _targetGroupId = -1;
	bool _wasTriggerActivated = false;
	int _copiedColorId = -1;

	float _fadeIn = 0.f, _hold = 0.f, _fadeOut = 0.f;
	int _pulseMode = 0, _pulseType = 0;
	bool _mainOnly = false, _detailOnly = false;
	bool _blending = false;
	bool _activateGroup = false;
	bool _spawnTriggered = false;
	bool _multiTriggered = false;
	float _spawnDelay = 0.f;

	ax::Vec2 _offset;

	int _easing = 0;
	float _easeRate = 2.f;

	bool _lockToPlayerX = false, _lockToPlayerY = false;
	bool _touchTriggered = false;
	int _itemID = 0;
	int _countTarget = 0;
	int _countCompare = 0; // 0 equal, 1 larger, 2 smaller
	float _rotation = 0.f;
	float _followXMod = 1.f;
	float _followYMod = 1.f;
	int _targetGroupId2 = -1;

private:
	virtual void updateTweenAction(float value, std::string_view key) override;

public:
	void triggerActivated(float);
	static EffectGameObject* create(std::string_view frame);
	static ax::Action* actionEasing(ax::ActionInterval* ac, int ease, float rate);
};