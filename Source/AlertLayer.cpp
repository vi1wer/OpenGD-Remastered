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

#include "AlertLayer.h"
#include "ButtonSprite.h"
#include "MenuItemSpriteExtra.h"

#include "2d/Menu.h"
#include "2d/Label.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "ui/UIScale9Sprite.h"
#include "base/Director.h"

#include <algorithm>
#include <string>
#include "GameToolbox/getTextureString.h"

USING_NS_AX;

bool AlertLayer::init(std::string_view title, std::string_view desc, std::string_view btn1Str, std::string_view btn2Str, float width, std::function<void(Node*)> btn1Callback, std::function<void(Node*)> btn2Callback)
{
	if (!PopupLayer::init())
		return false;

	const auto& winSize = Director::getInstance()->getWinSize();

	auto* descLabel = Label::createWithBMFont(GameToolbox::getTextureString("chatFont.fnt"), desc, TextHAlignment::CENTER);
	descLabel->setScale(0.8f);
	const float textW = std::clamp(width > 0.f ? width : 300.f, 180.f, 340.f);
	descLabel->setDimensions(textW - 40.f, 0);
	descLabel->setAlignment(TextHAlignment::CENTER, TextVAlignment::CENTER);

	const float titleH = 28.f;
	const float btnH = 36.f;
	const float padY = 18.f;
	const float descH = std::max(descLabel->getContentSize().height * descLabel->getScale(), 40.f);
	const float boxH = padY + titleH + 8.f + descH + 14.f + btnH + padY;
	const float boxW = textW;

	// Official FLAlertLayer uses GJ_square01 (blue), not the old square01_001.
	const Rect cap{0.f, 0.f, 80.f, 80.f};
	auto* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"), cap);
	if (!bg)
		bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("square01_001.png"), cap);
	if (!bg)
		return false;
	bg->setContentSize({boxW, boxH});
	bg->setPosition(winSize / 2);
	_mainLayer->addChild(bg, -1);

	auto* titleLabel = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), title);
	titleLabel->setScale(0.8f);
	titleLabel->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + boxH * 0.5f - padY - titleH * 0.5f});
	_mainLayer->addChild(titleLabel, 1);

	descLabel->setPosition({winSize.width * 0.5f, winSize.height * 0.5f + 6.f});
	_mainLayer->addChild(descLabel, 1);

	auto* menu = Menu::create();
	menu->setPosition({winSize.width * 0.5f, winSize.height * 0.5f - boxH * 0.5f + padY + btnH * 0.5f});
	_mainLayer->addChild(menu, 2);

	auto btn1Cb = btn1Callback ? btn1Callback : [this](Node*) { close(); };
	_btn1 = MenuItemSpriteExtra::create(
		ButtonSprite::create(btn1Str, 0x50, 0, 0.8f, false, GameToolbox::getTextureString("goldFont.fnt"),
			GameToolbox::getTextureString("GJ_button_01.png"), 28.f),
		btn1Cb);
	menu->addChild(_btn1);

	if (!btn2Str.empty())
	{
		auto btn2Cb = btn2Callback ? btn2Callback : [this](Node*) { close(); };
		_btn2 = MenuItemSpriteExtra::create(
			ButtonSprite::create(btn2Str, 0x50, 0, 0.8f, false, GameToolbox::getTextureString("goldFont.fnt"),
				GameToolbox::getTextureString("GJ_button_01.png"), 28.f),
			btn2Cb);
		menu->addChild(_btn2);
		menu->alignItemsHorizontallyWithPadding(16.f);
	}

	if (SpriteFrameCache::getInstance()->getSpriteFrameByName("GJ_closeBtn_001.png"))
	{
		auto* closeSpr = Sprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
		if (closeSpr)
		{
			closeSpr->setStretchEnabled(false);
			auto* closeBtn = MenuItemSpriteExtra::create(closeSpr, [this](Node*) { close(); });
			closeBtn->setScale(0.7f);
			auto* closeMenu = Menu::createWithItem(closeBtn);
			closeMenu->setPosition({winSize.width * 0.5f - boxW * 0.5f + 8.f, winSize.height * 0.5f + boxH * 0.5f - 8.f});
			_mainLayer->addChild(closeMenu, 5);
		}
	}

	return true;
}

AlertLayer* AlertLayer::create(std::string_view title, std::string_view desc, std::string_view btn1, std::string_view btn2, float width, std::function<void(Node*)> btn1Callback, std::function<void(Node*)> btn2Callback)
{
	auto pRet = new (std::nothrow) AlertLayer();

	if (pRet && pRet->init(title, desc, btn1, btn2, width, btn1Callback, btn2Callback))
	{
		pRet->autorelease();
		return pRet;
	}
	AX_SAFE_DELETE(pRet);
	return nullptr;
}

AlertLayer* AlertLayer::create(std::string_view title, std::string_view desc, std::string_view btn1, std::string_view btn2, std::function<void(Node*)> btn1Callback, std::function<void(Node*)> btn2Callback)
{
	return AlertLayer::create(title, desc, btn1, btn2, 300.f, btn1Callback, btn2Callback);
}

AlertLayer* AlertLayer::create(std::string_view title, std::string_view desc, std::string_view btn1, float width, std::function<void(Node*)> btn1Callback)
{
	return AlertLayer::create(title, desc, btn1, "", width, btn1Callback, nullptr);
}

AlertLayer* AlertLayer::create(std::string_view title, std::string_view desc, std::string_view btn1, std::function<void(Node*)> btn1Callback)
{
	return AlertLayer::create(title, desc, btn1, "", 300.f, btn1Callback, nullptr);
}

AlertLayer* AlertLayer::create(std::string_view title, std::string_view desc, float width)
{
	return AlertLayer::create(title, desc, "OK", "", width, nullptr, nullptr);
}

AlertLayer* AlertLayer::create(std::string_view title, std::string_view desc)
{
	return AlertLayer::create(title, desc, "OK", "", 300.f, nullptr, nullptr);
}

void AlertLayer::setBtn1Callback(std::function<void(Node*)> btn1Callback)
{
	_btn1->setCallback(btn1Callback);
}

void AlertLayer::setBtn2Callback(std::function<void(Node*)> btn2Callback)
{
	_btn2->setCallback(btn2Callback);
}
