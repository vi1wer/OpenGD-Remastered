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

#include "UILayer.h"
#include "PlayLayer.h"

#include "EventListenerTouch.h"
#include "EventDispatcher.h"
#include "base/Director.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/Label.h"
#include "2d/SpriteFrameCache.h"
#include "MenuItemSpriteExtra.h"
#include "ButtonSprite.h"
#include "GameToolbox/getTextureString.h"

UILayer* UILayer::create()
{
	UILayer* pRet = new UILayer();
	if (pRet->init())
	{
		pRet->autorelease();
		return pRet;
	}
	else
	{
		delete pRet;
		pRet = nullptr;
		return nullptr;
	}
}

bool UILayer::init()
{
	if (!ax::Layer::init()) return false;

	auto dir = ax::Director::getInstance();
	auto listener = ax::EventListenerTouchOneByOne::create();

	_listener = listener;

	listener->setEnabled(true);
	listener->setSwallowTouches(true);

	// trigger when you start touch
	listener->onTouchBegan = AX_CALLBACK_2(UILayer::onTouchBegan, this);
	listener->onTouchEnded = AX_CALLBACK_2(UILayer::onTouchEnded, this);

	dir->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);

	const auto& winSize = dir->getWinSize();
	ax::Node* pauseSpr = nullptr;
	if (ax::SpriteFrameCache::getInstance()->getSpriteFrameByName("GJ_pauseBtn_001.png"))
	{
		auto* spr = ax::Sprite::createWithSpriteFrameName("GJ_pauseBtn_001.png");
		if (spr)
		{
			spr->setStretchEnabled(false);
			pauseSpr = spr;
		}
	}
	if (!pauseSpr)
		pauseSpr = ButtonSprite::create("II");

	auto* pauseBtn = MenuItemSpriteExtra::create(pauseSpr, [](ax::Node*) {
		if (auto* pl = PlayLayer::getInstance())
			pl->pauseGame();
	});
	auto* pauseMenu = ax::Menu::create(pauseBtn, nullptr);
	pauseMenu->setPosition({0.f, 0.f});
	pauseBtn->setPosition({45.f, winSize.height - 45.f});
	addChild(pauseMenu, 40);

	return true;
}

void UILayer::setPracticeHintVisible(bool visible)
{
	if (!visible)
	{
		if (_practiceHint)
			_practiceHint->setVisible(false);
		return;
	}

	if (_practiceHint)
	{
		_practiceHint->setVisible(true);
		return;
	}

	const auto& winSize = ax::Director::getInstance()->getWinSize();
	_practiceHint = ax::Node::create();
	_practiceHint->setPosition({winSize.width * 0.5f, 36.f});
	addChild(_practiceHint, 45);

	auto makeHintBtn = [](const char* frame, const char* fallback) -> ax::Node* {
		if (ax::SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		{
			auto* spr = ax::Sprite::createWithSpriteFrameName(frame);
			if (spr)
			{
				spr->setStretchEnabled(false);
				return spr;
			}
		}
		return ButtonSprite::create(fallback);
	};

	auto* addSpr = makeHintBtn("GJ_checkpointBtn_001.png", "Z");
	auto* removeSpr = makeHintBtn("GJ_removeCheckBtn_001.png", "X");
	auto* addBtn = MenuItemSpriteExtra::create(addSpr, [](ax::Node*) {
		if (auto* pl = PlayLayer::getInstance())
			pl->markCheckpoint();
	});
	auto* removeBtn = MenuItemSpriteExtra::create(removeSpr, [](ax::Node*) {
		if (auto* pl = PlayLayer::getInstance())
			pl->removeCheckpoint();
	});

	const float addW = addSpr ? addSpr->getContentSize().width * addSpr->getScaleX() : 40.f;
	const float removeW = removeSpr ? removeSpr->getContentSize().width * removeSpr->getScaleX() : 40.f;
	addBtn->setPosition({-addW * 0.5f, 8.f});
	removeBtn->setPosition({removeW * 0.5f, 8.f});

	auto* menu = ax::Menu::create(addBtn, removeBtn, nullptr);
	menu->setPosition({0.f, 0.f});
	_practiceHint->addChild(menu, 2);

	auto addKeyLabel = [&](const char* text, float x) {
		auto* lbl = ax::Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), text);
		if (!lbl)
			return;
		lbl->setScale(0.32f);
		lbl->setPosition({x, -22.f});
		_practiceHint->addChild(lbl, 3);
	};
	addKeyLabel("Z", -addW * 0.5f);
	addKeyLabel("X", removeW * 0.5f);
}

bool UILayer::onTouchBegan(ax::Touch* touch, ax::Event* event)
{
	auto pl = PlayLayer::getInstance();
	if (!pl || pl->isPaused())
		return false;

	const auto loc = touch->getLocation();
	const auto& winSize = ax::Director::getInstance()->getWinSize();
	if (loc.x < 72.f && loc.y > winSize.height - 72.f)
		return false;
	if (pl->isPracticeMode() && loc.y < 78.f)
		return false;

	pl->_player1->pushButton();
	if (pl->_isDualMode) pl->_player2->pushButton();
	return true;
}

void UILayer::onTouchEnded(ax::Touch* touch, ax::Event* event)
{
	auto pl = PlayLayer::getInstance();
	if (!pl || pl->isPaused())
		return;
	pl->_player1->releaseButton();
	if (pl->_isDualMode) pl->_player2->releaseButton();
}