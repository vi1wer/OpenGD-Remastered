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

#include "CircleWave.h"
#include "Director.h"
#include "2d/ActionInstant.h"
#include "2d/ActionManager.h"
#include "2d/ActionEase.h"
#include "base/Types.h"
#include <algorithm>

USING_NS_AX;

void CircleWave::setColor(Color4B col) 
{
	this->_color = {col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f};
}

Color4B CircleWave::getColor() 
{
	return {(uint8_t)(_color.r * 255.f), (uint8_t)(_color.g * 255.f), (uint8_t)(_color.b * 255.f), (uint8_t)(_color.a * 255.f)};
}

bool CircleWave::init(float duration, Color4B color, float radiusMin, float radiusMax, bool easing, float lineWidth, bool filled) 
{
	if (!DrawNode::init()) return false;

	this->setColor(color);
	this->_radius = radiusMin;
	this->_lineWidth = lineWidth;
	this->_filled = filled;
	this->_followedNode = nullptr;
	setLineWidth(std::max(lineWidth, 2.f));
	setBlendFunc(BlendFunc::ADDITIVE);

	if (easing)
	{
		this->_color.a = 0;
		auto radAction = ActionTween::create(duration, "radius", radiusMin, radiusMax);
		auto opacityAction = ActionTween::create(duration / 2, "opacity", 0.0, 1.0);
		auto opacityAction2 = ActionTween::create(duration / 2, "opacity", 1.0, 0.0);
		auto seq = Sequence::create(opacityAction, opacityAction2, CallFunc::create([&]() {
			this->removeFromParent();
		}), nullptr);
		auto action = Spawn::create(radAction, seq, nullptr);
		Director::getInstance()->getActionManager()->addAction(action, this, false);
	}
	else 
	{
		auto radAction = EaseOut::create(ActionTween::create(duration, "radius", radiusMin, radiusMax), 2.0f);
		auto opacityAction = EaseOut::create(ActionTween::create(duration / 2, "opacity", this->_color.a, 0.0), 2.0f);
		auto spawn = Spawn::create(radAction, opacityAction, nullptr);
		auto action = Sequence::create(spawn, CallFunc::create([&](){ this->removeFromParent();}), nullptr);
		Director::getInstance()->getActionManager()->addAction(action, this, false);
	}

	this->scheduleUpdate();

	return true;
}

void CircleWave::updateTweenAction(float value, std::string_view key)
{
	if (key == "radius") {
		this->_radius = value;
	} else if (key == "opacity") {
		this->_color.a = value;
	}
}

void CircleWave::update(float dt)
{
	if (this->_followedNode)
		this->setPosition(_followedNode->getPosition());

	this->clear();

	const Color4B col(static_cast<uint8_t>(std::clamp(_color.r, 0.f, 1.f) * 255.f),
					  static_cast<uint8_t>(std::clamp(_color.g, 0.f, 1.f) * 255.f),
					  static_cast<uint8_t>(std::clamp(_color.b, 0.f, 1.f) * 255.f),
					  static_cast<uint8_t>(std::clamp(_color.a, 0.f, 1.f) * 255.f));

	const int segs = this->_radius <= 400.f ? 48 : 64;
	if (this->_filled)
	{
		this->drawSolidCircle(Vec2::ZERO, this->_radius, 0.0f, segs, col);
	}
	else
	{
		// Thicker ring by drawing several concentric circles (GD-style pulse).
		const float thickness = std::max(2.f, _lineWidth);
		setLineWidth(2.f);
		for (float o = 0.f; o < thickness; o += 1.5f)
		{
			const float r = std::max(1.f, this->_radius - o);
			this->drawCircle(Vec2::ZERO, r, 0.0f, segs, false, col);
		}
	}
}

void CircleWave::followNode(Node* node) {
	this->_followedNode = node;
}

CircleWave* CircleWave::create(float duration, Color4B color, float radiusMin, float radiusMax, bool easing, bool filled, float lineWidth)
{
	auto pRet = new(std::nothrow) CircleWave();

	if (pRet && pRet->init(duration, color, radiusMin, radiusMax, easing, lineWidth, filled))
	{
		pRet->autorelease();
		return pRet;
	} 
	else
	{
		AX_SAFE_DELETE(pRet);
		return nullptr;
	}
}