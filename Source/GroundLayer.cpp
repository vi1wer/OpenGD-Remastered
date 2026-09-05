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

#include "GroundLayer.h"
#include "PlayLayer.h"
#include "base/Director.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/conv.h"
#include "fmt/format.h"

USING_NS_AX;

bool GroundLayer::init(int groundID)
{
	if (!Layer::init()) return false;

	_groundID = groundID > 0 ? groundID : 1;
	auto winSize = Director::getInstance()->getWinSize();

	auto name = fmt::format("groundSquare_{:02}_001.png", _groundID);
	this->_sprite = Sprite::create(GameToolbox::getTextureString(name));
	if (!this->_sprite)
	{
		auto name2 = fmt::format("groundSquare_{:02}_001.png", 1);
		this->_sprite = Sprite::create(GameToolbox::getTextureString(name2));
		_groundID = 1;
	}
	if (!this->_sprite)
		return false;
	_sprite->setStretchEnabled(false);
	this->m_fOneGroundSize = this->_sprite->getTextureRect().size.width;
	if (auto* tex = this->_sprite->getTexture())
	{
		tex->setTexParameters(
			{backend::SamplerFilter::NEAREST, backend::SamplerFilter::NEAREST, backend::SamplerAddressMode::REPEAT,
			 backend::SamplerAddressMode::REPEAT});
	}
	this->_sprite->setTextureRect({0, 0, winSize.width + this->m_fOneGroundSize, _sprite->getTextureRect().size.height});
	this->_sprite->setAnchorPoint({0, 0});
	this->_sprite->setPosition({0, -50});
	this->_sprite->setColor({0, 102, 255});
	this->addChild(this->_sprite);

	auto line = Sprite::createWithSpriteFrameName("floorLine_001.png");
	line->setStretchEnabled(false);
	line->setBlendFunc(GameToolbox::getBlending());
	this->addChild(line);
	line->setPosition({winSize.width / 2, this->_sprite->getContentSize().height + this->_sprite->getPositionY()});

	auto gradient1 = Sprite::createWithSpriteFrameName("groundSquareShadow_001.png");
	gradient1->setStretchEnabled(false);
	this->addChild(gradient1);
	gradient1->setScale(0.7f);
	gradient1->setPositionY(33);

	auto gradient2 = Sprite::createWithSpriteFrameName("groundSquareShadow_001.png");
	gradient2->setStretchEnabled(false);
	this->addChild(gradient2);
	gradient2->setScale(0.7f);
	gradient2->setFlippedX(true);
	gradient2->setPositionX(winSize.width);
	gradient2->setPositionY(33);

	this->m_fSpeed = 5.770002f;

	//scheduleUpdate();

	return true;
}

void GroundLayer::setGroundID(int groundID)
{
	if (groundID < 1)
		groundID = 1;
	_groundID = groundID;
	if (!_sprite)
		return;

	const auto winSize = Director::getInstance()->getWinSize();
	auto name = fmt::format("groundSquare_{:02}_001.png", _groundID);
	auto* tmp = Sprite::create(GameToolbox::getTextureString(name));
	if (!tmp)
	{
		_groundID = 1;
		tmp = Sprite::create(GameToolbox::getTextureString("groundSquare_01_001.png"));
	}
	if (!tmp || !tmp->getTexture())
		return;

	auto* tex = tmp->getTexture();
	// Copy tex params on a per-use basis; do not leave menu sprites pointing at editor ground.
	tex->setTexParameters(
		{backend::SamplerFilter::NEAREST, backend::SamplerFilter::NEAREST, backend::SamplerAddressMode::REPEAT,
		 backend::SamplerAddressMode::REPEAT});
	_sprite->setTexture(tex);
	m_fOneGroundSize = tmp->getContentSize().width;
	_sprite->setTextureRect({0, 0, winSize.width + m_fOneGroundSize, tmp->getContentSize().height});
}

void GroundLayer::resetMenuAppearance()
{
	setGroundID(1);
	if (_sprite)
		_sprite->setColor({0, 102, 255});
}

void GroundLayer::updateForWinSize()
{
	if (!_sprite)
		return;

	const auto winSize = Director::getInstance()->getWinSize();
	const float height = _sprite->getTextureRect().size.height;
	_sprite->setTextureRect({0, 0, winSize.width + m_fOneGroundSize, height});

	for (auto* child : getChildren())
	{
		if (child == _sprite)
			continue;
		auto* spr = dynamic_cast<Sprite*>(child);
		if (!spr)
			continue;
		if (spr->isFlippedX())
			spr->setPositionX(winSize.width);
		else if (spr->getPositionY() > 40.f)
			spr->setPositionX(winSize.width * 0.5f);
	}
}

void GroundLayer::updateTweenAction(float value, std::string_view key)
{
	if (key == "y") setPositionY(value);
}

void GroundLayer::update(float dt)
{
	if (_followPlayLayerColors)
	{
		if (auto* pl = BaseGameLayer::getInstance())
		{
			if (pl->_colorChannels.contains(1001) && _sprite)
				_sprite->setColor(pl->colorForChannel(1001));
		}
	}

	if (!_sprite)
		return;

	this->_sprite->setPositionX(this->_sprite->getPositionX() - dt * this->m_fSpeed);

	if (this->_sprite->getPositionX() <= -128.0f) this->_sprite->setPositionX(0);
	if (this->_sprite->getPositionX() >= 64.0f) this->_sprite->setPositionX(-64);
}

GroundLayer* GroundLayer::create(int groundID)
{
	GroundLayer* pRet = new (std::nothrow) GroundLayer();

	if (pRet && pRet->init(groundID))
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