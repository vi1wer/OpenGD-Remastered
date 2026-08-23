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

#include "EffectGameObject.h"
#include "2d/ActionEase.h"
#include "2d/ActionInstant.h"
#include "2d/ActionInterval.h"
#include "2d/ParticleSystemQuad.h"
#include "BaseGameLayer.h"
#include "ColorAction.h"
#include "SpriteColor.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/log.h"
#include "GroupColorAction.h"
#include "PlayLayer.h"
#include "UTF8.h"
#include <cmath>

USING_NS_AX;

Action* EffectGameObject::actionEasing(ActionInterval* ac, int ease, float rate)
{
	switch (ease)
	{
	case 1:
		return EaseInOut::create(ac, rate);
		break;
	case 2:
		return EaseIn::create(ac, rate);
		break;
	case 3:
		return EaseOut::create(ac, rate);
		break;
	case 4:
		return EaseElasticInOut::create(ac, rate);
		break;
	case 5:
		return EaseElasticIn::create(ac, rate);
		break;
	case 6:
		return EaseElasticOut::create(ac, rate);
		break;
	case 7:
		return EaseBounceInOut::create(ac);
		break;
	case 8:
		return EaseBounceIn::create(ac);
		break;
	case 9:
		return EaseBounceOut::create(ac);
		break;
	case 10:
		return EaseExponentialInOut::create(ac);
		break;
	case 11:
		return EaseExponentialIn::create(ac);
		break;
	case 12:
		return EaseExponentialOut::create(ac);
		break;
	case 13:
		return EaseSineInOut::create(ac);
		break;
	case 14:
		return EaseSineIn::create(ac);
		break;
	case 15:
		return EaseSineOut::create(ac);
		break;
	case 16:
		return EaseBackInOut::create(ac);
		break;
	case 17:
		return EaseBackIn::create(ac);
		break;
	case 18:
		return EaseBackOut::create(ac);
		break;
	}
	return ac;
}

void EffectGameObject::triggerActivated(float)
{
	if (!_bgl)
		return;
	if (_wasTriggerActivated && !_multiTriggered)
		return;

	auto pl = PlayLayer::getInstance();

	_wasTriggerActivated = true;

	auto id = getID();

	if (id == 29)
		_targetColorId = 1000;
	else if (id == 30)
		_targetColorId = 1001;

	if (!_bgl->_colorChannels.contains(_targetColorId))
	{
		_bgl->_colorChannels.insert({_targetColorId, SpriteColor(Color3B::WHITE, 255, 0)});
		_bgl->_originalColors.insert({_targetColorId, SpriteColor(Color3B::WHITE, 255, 0)});
	}

	switch (id)
	{
	case 30:
	case 29:
	case 899: {

		_bgl->_colorChannels.at(_targetColorId)._applyHsv = false;
		if (_copiedColorId > -1)
		{
			int cycles = 0;

			if (!_bgl->_colorChannels.contains(_copiedColorId))
			{
				_bgl->_colorChannels.insert({_copiedColorId, SpriteColor(Color3B::WHITE, 255, 0)});
				_bgl->_originalColors.insert({_copiedColorId, SpriteColor(Color3B::WHITE, 255, 0)});
			}

			auto colorChannel = _bgl->_colorChannels.at(_copiedColorId);

			while (colorChannel._copyingColorID != -1 && cycles < 3)
			{
				cycles++;
				colorChannel = _bgl->_colorChannels[colorChannel._copyingColorID];
			}

			_color = colorChannel._color;

			GameToolbox::applyHSV(_hsv, &_color);

			_bgl->_colorChannels.at(_targetColorId)._applyHsv = true;
		}

		if (auto* colorAction = ColorAction::create(
				_duration, &_bgl->_colorChannels.at(_targetColorId), _bgl->_colorChannels.at(_targetColorId)._color, _color,
				_bgl->_colorChannels.at(_targetColorId)._opacity, _opacity * 255.0f, _copiedColorId, &_hsv))
			_bgl->runAction(colorAction);
		_bgl->_colorChannels.at(_targetColorId)._blending = _blending;
	}
	break;
	case 22:
		if (pl)
			pl->_enterEffectID = 1;
		break;
	case 23:
		if (pl)
			pl->_enterEffectID = 5;
		break;
	case 24:
		if (pl)
			pl->_enterEffectID = 4;
		break;
	case 25:
		if (pl)
			pl->_enterEffectID = 6;
		break;
	case 26:
		if (pl)
			pl->_enterEffectID = 7;
		break;
	case 27:
		if (pl)
			pl->_enterEffectID = 2;
		break;
	case 28:
		if (pl)
			pl->_enterEffectID = 3;
		break;
	case 901: {
		float dur = _duration;
		if ((_lockToPlayerX || _lockToPlayerY) && dur <= 0.f)
			dur = 9999.f;
		_bgl->runMoveCommand(dur, _offset, _easing, _easeRate, _targetGroupId, _lockToPlayerX, _lockToPlayerY);
	}
	break;
	case 1007:
		if (!_bgl->_groups.contains(_targetGroupId))
		{
			GroupProperties gp;
			_bgl->_groups.insert({_targetGroupId, gp});
		}
		if (_duration <= 0.f || !isRunning())
			_bgl->_groups[_targetGroupId]._alpha = _opacity;
		else
			runAction(ActionTween::create(_duration, "fade", _bgl->_groups[_targetGroupId]._alpha, _opacity));
		break;
	case 1006: {

		if (!_bgl->_colorChannels.contains(_targetGroupId))
		{
			_bgl->_colorChannels.insert({_targetGroupId, SpriteColor(Color3B::WHITE, 255, 0)});
			_bgl->_originalColors.insert({_targetGroupId, SpriteColor(Color3B::WHITE, 255, 0)});
		}
		Color3B original = _bgl->_colorChannels.at(_targetGroupId)._color;

		Color3B target = _color;

		if (_pulseMode) // hsv
		{
			if (_copiedColorId == -1)
				target = original;
			else
			{
				if (!_bgl->_colorChannels.contains(_copiedColorId))
				{
					_bgl->_colorChannels.insert({_copiedColorId, SpriteColor(Color3B::WHITE, 255, 0)});
					_bgl->_originalColors.insert({_copiedColorId, SpriteColor(Color3B::WHITE, 255, 0)});
				}
				int cycles = 0;

				auto colorChannel = _bgl->_colorChannels.at(_targetGroupId);

				while (colorChannel._copyingColorID != -1 && cycles < 3)
				{
					cycles++;
					colorChannel = _bgl->_colorChannels[colorChannel._copyingColorID];
				}

				target = colorChannel._color;
			}

			GameToolbox::applyHSV(_hsv, &target);
		}

		ax::Sequence* seq;

		if (_pulseType == 0)
		{
			auto* colPointer = &_bgl->_colorChannels.at(_targetGroupId);
			ax::Vector<FiniteTimeAction*> pulseActions;
			if (auto* a = ColorAction::create(_fadeIn > 0.f ? _fadeIn : 0.01f, colPointer, original, target))
				pulseActions.pushBack(a);
			if (auto* a = ColorAction::create(_hold > 0.f ? _hold : 0.01f, colPointer, target, target))
				pulseActions.pushBack(a);
			if (auto* a = ColorAction::create(_fadeOut > 0.f ? _fadeOut : 0.01f, colPointer, target, original))
				pulseActions.pushBack(a);
			if (pulseActions.empty())
				break;
			seq = ax::Sequence::create(pulseActions);
		}
		else
		{
			if (!_bgl->_groups.contains(_targetGroupId))
			{
				GroupProperties gp;
				_bgl->_groups.insert({_targetGroupId, gp});
			}
			auto groupPointer = &_bgl->_groups.at(_targetGroupId);

			if (!_mainOnly && !_detailOnly)
				groupPointer->groupState = GroupProperties::GroupState::MAIN_DETAIL;
			else if (_mainOnly)
				groupPointer->groupState = GroupProperties::GroupState::MAIN_ONLY;
			else
				groupPointer->groupState = GroupProperties::GroupState::DETAIL_ONLY;

			ax::Vector<FiniteTimeAction*> pulseActions;
			if (auto* a = GroupColorAction::create(_fadeIn > 0.f ? _fadeIn : 0.01f, groupPointer, original, target, false))
				pulseActions.pushBack(a);
			if (auto* a = GroupColorAction::create(_hold > 0.f ? _hold : 0.01f, groupPointer, target, target, false))
				pulseActions.pushBack(a);
			if (auto* a = GroupColorAction::create(_fadeOut > 0.f ? _fadeOut : 0.01f, groupPointer, target, original, true))
				pulseActions.pushBack(a);
			if (pulseActions.empty())
				break;
			seq = ax::Sequence::create(pulseActions);
		}

		if (seq)
			_bgl->runAction(seq);
		break;
	}
	case 1049:
		if (!_bgl->_groups.contains(_targetGroupId))
			break;
		if (_activateGroup)
		{
			for (GameObject* obj : _bgl->_groups[_targetGroupId]._objects)
			{
				if (obj)
					obj->_toggledOn = true;
			}
		}
		else
		{
			for (GameObject* obj : _bgl->_groups[_targetGroupId]._objects)
			{
				if (!obj)
					continue;
				obj->_toggledOn = false;
				obj->removeFromGameLayer();
			}
		}

		break;
	case 1268: {
		const int groupId = _targetGroupId;
		auto fireSpawn = [bgl = _bgl, groupId](float dt) {
			if (!bgl || !bgl->_groups.contains(groupId))
				return;
			for (GameObject* gameObj : bgl->_groups[groupId]._objects)
			{
				if (gameObj && gameObj->_isTrigger)
					static_cast<EffectGameObject*>(gameObj)->triggerActivated(dt);
			}
		};
		if (pl)
			pl->scheduleOnce([fireSpawn](float dt) { fireSpawn(dt); }, std::max(_spawnDelay, 0.f),
							 StringUtils::format("spawn_%p", this));
		else
			fireSpawn(0.f);
		break;
	}
	case 31:
	case 32:
	case 33:
	case 34:
	case 104:
	case 105:
	case 221:
	case 717:
	case 718:
	case 743:
	case 744:
	case 915: {
		if (!_bgl->_colorChannels.contains(_targetColorId))
		{
			_bgl->_colorChannels.insert({_targetColorId, SpriteColor(Color3B::WHITE, 255, 0)});
			_bgl->_originalColors.insert({_targetColorId, SpriteColor(Color3B::WHITE, 255, 0)});
		}
		if (auto* colorAction = ColorAction::create(
				_duration, &_bgl->_colorChannels.at(_targetColorId), _bgl->_colorChannels.at(_targetColorId)._color, _color,
				_bgl->_colorChannels.at(_targetColorId)._opacity, _opacity * 255.0f, _copiedColorId, &_hsv))
			_bgl->runAction(colorAction);
		_bgl->_colorChannels.at(_targetColorId)._blending = _blending;
		break;
	}
	case 1346: {
		if (!_bgl->_groups.contains(_targetGroupId))
			break;
		const float degrees = _rotation != 0.f ? _rotation : _offset.x;
		const float dur = _duration > 0.f ? _duration : 0.01f;
		for (GameObject* obj : _bgl->_groups[_targetGroupId]._objects)
		{
			if (!obj)
				continue;
			auto* rot = RotateBy::create(dur, degrees);
			obj->runAction(actionEasing(rot, _easing, _easeRate));
		}
		break;
	}
	case 1347:
	case 1814:
		_bgl->runFollowCommand(_duration > 0.f ? _duration : 9999.f, _targetGroupId,
							   _lockToPlayerX || id == 1814, _lockToPlayerY || id == 1814);
		break;
	case 1520:
		if (pl)
		{
			pl->_shakeTime = _duration > 0.f ? _duration : 0.4f;
			pl->_shakeStrength = _offset.x != 0.f ? std::abs(_offset.x) : 5.f;
		}
		break;
	case 1585:
		if (_bgl->_groups.contains(_targetGroupId))
		{
			for (GameObject* obj : _bgl->_groups[_targetGroupId]._objects)
			{
				if (!obj)
					continue;
				if (obj->_particle)
				{
					obj->_particle->resetSystem();
					obj->_particle->resumeEmissions();
				}
				else
				{
					obj->runAction(Sequence::create(
						FadeTo::create(0.08f, 80), FadeTo::create(0.08f, 255), nullptr));
				}
			}
		}
		break;
	case 1595:
		if (_bgl->_groups.contains(_targetGroupId))
		{
			for (GameObject* gameObj : _bgl->_groups[_targetGroupId]._objects)
			{
				if (gameObj && gameObj->_isTrigger)
					static_cast<EffectGameObject*>(gameObj)->triggerActivated(0.f);
			}
		}
		break;
	case 1611:
	case 1811:
		if (pl)
			pl->tryActivateCountTrigger(this);
		break;
	case 1616:
		_bgl->stopGroupActions(_targetGroupId);
		break;
	case 1812:
		if (_bgl->_groups.contains(_targetGroupId))
		{
			for (GameObject* gameObj : _bgl->_groups[_targetGroupId]._objects)
			{
				if (gameObj && gameObj->_isTrigger)
					static_cast<EffectGameObject*>(gameObj)->triggerActivated(0.f);
			}
		}
		break;
	case 1815:
	case 1912:
		if (_bgl->_groups.contains(_targetGroupId))
		{
			for (GameObject* gameObj : _bgl->_groups[_targetGroupId]._objects)
			{
				if (gameObj && gameObj->_isTrigger)
					static_cast<EffectGameObject*>(gameObj)->triggerActivated(0.f);
			}
		}
		break;
	case 1817: {
		if (pl)
		{
			pl->_itemCounts[_itemID] += _activateGroup ? 1 : -1;
			for (auto& [gid, group] : _bgl->_groups)
			{
				for (GameObject* gameObj : group._objects)
				{
					if (!gameObj || !gameObj->_isTrigger)
						continue;
					const int tid = gameObj->getID();
					if (tid == 1611 || tid == 1811)
						pl->tryActivateCountTrigger(static_cast<EffectGameObject*>(gameObj));
				}
			}
		}
		break;
	}
	case 1818:
		if (pl)
			pl->_enterEffectID = 1;
		break;
	case 1819:
		if (pl)
			pl->_enterEffectID = 0;
		break;
	case 900:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
		if (pl)
			pl->_enterEffectID = (id % 7) + 1;
		break;
	case 1913:
	case 1914:
	case 1916:
	case 1917:
	case 1931:
	case 1932:
	case 1934:
	case 1935:
	case 2015:
	case 2016:
	case 2062:
	case 2067:
	case 2068:
	case 2701:
	case 2702:
		if (_targetGroupId >= 0 && _bgl->_groups.contains(_targetGroupId))
		{
			for (GameObject* obj : _bgl->_groups[_targetGroupId]._objects)
			{
				if (!obj)
					continue;
				if (_activateGroup)
					obj->_toggledOn = true;
				else
				{
					obj->_toggledOn = false;
					obj->removeFromGameLayer();
				}
			}
		}
		if (_duration > 0.f && _offset != Vec2::ZERO)
			_bgl->runMoveCommand(_duration, _offset, _easing, _easeRate, _targetGroupId, _lockToPlayerX, _lockToPlayerY);
		break;
	}
}

void EffectGameObject::updateTweenAction(float value, std::string_view key)
{
	if (!_bgl)
		return;

	auto setChannel = [&](int id, auto&& fn) {
		auto it = _bgl->_colorChannels.find(id);
		if (it != _bgl->_colorChannels.end())
			fn(it->second);
	};

	if (key == "col1")
		setChannel(_targetColorId, [&](SpriteColor& c) { c._color.r = static_cast<uint8_t>(value); });
	else if (key == "col2")
		setChannel(_targetColorId, [&](SpriteColor& c) { c._color.g = static_cast<uint8_t>(value); });
	else if (key == "col3")
		setChannel(_targetColorId, [&](SpriteColor& c) { c._color.b = static_cast<uint8_t>(value); });
	else if (key == "col4")
		setChannel(_targetColorId, [&](SpriteColor& c) { c._opacity = value; });
	else if (key == "pul1")
		setChannel(_targetGroupId, [&](SpriteColor& c) { c._color.r = static_cast<uint8_t>(value); });
	else if (key == "pul2")
		setChannel(_targetGroupId, [&](SpriteColor& c) { c._color.g = static_cast<uint8_t>(value); });
	else if (key == "pul3")
		setChannel(_targetGroupId, [&](SpriteColor& c) { c._color.b = static_cast<uint8_t>(value); });
	else if (key == "fade")
	{
		auto it = _bgl->_groups.find(_targetGroupId);
		if (it != _bgl->_groups.end())
			it->second._alpha = value;
	}
}

EffectGameObject* EffectGameObject::create(std::string_view frame)
{
	auto pRet = new (std::nothrow) EffectGameObject();

	if (pRet && pRet->init(frame))
	{
		pRet->_bgl = BaseGameLayer::getInstance();
		pRet->setVisible(false);
		pRet->setOpacity(0);
		pRet->autorelease();
		return pRet;
	}
	AX_SAFE_DELETE(pRet);
	return nullptr;
}