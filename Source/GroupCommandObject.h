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

#include "2d/ActionEase.h"
#include "2d/ActionTween.h"

class GroupCommandObject : public ax::Node, public ax::ActionTweenDelegate
{
  public:
	ax::Point _oldPos;
	ax::Point _currentOffset;
	ax::Point _newPos;
	ax::Point _unkPoint;
	ax::Point _finalPoint;

	int _groupID = 0;
	int _actionID = 0;

	float _elapsed = 0.f, _duration = 0.f, _delta1 = 0.f, _delta2 = 0.f;
	float _easeAmt = 2.f;
	int _easeType = 0;

	bool _followPlayerX = false, _followPlayerY = false;
	bool _actionDone = false, _actionDoneForNextLoop = false;

	ax::Action* _action1 = nullptr;
	ax::Action* _action2 = nullptr;

	void runMoveCommand(float duration, ax::Point offsetPos, int easeType, float easeAmt);
	void step(float dt);
	virtual void updateTweenAction(float value, std::string_view key) override;

	static GroupCommandObject *create();
};