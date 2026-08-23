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

#include "ComingSoonLayer.h"

#include "ButtonSprite.h"
#include "MenuItemSpriteExtra.h"
#include "GameToolbox/getTextureString.h"

#include "2d/ActionEase.h"
#include "2d/ActionInterval.h"
#include "2d/Label.h"
#include "2d/Layer.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "ui/UIScale9Sprite.h"
#include "GameToolbox/nodes.h"

#include <fmt/format.h>

USING_NS_AX;

namespace
{
	Sprite* frameSprite(const char* name)
	{
		if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(name))
			return nullptr;
		auto* spr = Sprite::createWithSpriteFrameName(name);
		if (spr)
			spr->setStretchEnabled(false);
		return spr;
	}

	void addCorner(Node* parent, const Vec2& pos, bool flipX, bool flipY)
	{
		auto* corner = frameSprite("rewardCorner_001.png");
		if (!corner)
			return;
		corner->setPosition(pos);
		corner->setFlippedX(flipX);
		corner->setFlippedY(flipY);
		parent->addChild(corner, 2);
	}
}

ComingSoonLayer* ComingSoonLayer::create(std::string_view featureName)
{
	auto* ret = new (std::nothrow) ComingSoonLayer();
	if (ret && ret->init(featureName))
	{
		ret->autorelease();
		return ret;
	}
	AX_SAFE_DELETE(ret);
	return nullptr;
}

bool ComingSoonLayer::init(std::string_view featureName)
{
	if (!PopupLayer::init())
		return false;

	const auto& winSize = Director::getInstance()->getWinSize();
	const Vec2 center = winSize / 2;
	constexpr float panelW = 420.f;
	constexpr float panelH = 268.f;

	Node* bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square02.png"));
	if (!bg)
		bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"));
	if (!bg)
		bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("square01_001.png"));
	if (!bg)
		bg = ui::Scale9Sprite::createWithSpriteFrameName("square01_001.png");
	if (!bg)
	{
		auto* fallback = LayerColor::create(Color4B(40, 70, 140, 255), panelW, panelH);
		fallback->setIgnoreAnchorPointForPosition(false);
		fallback->setAnchorPoint({0.5f, 0.5f});
		bg = fallback;
	}
	bg->setContentSize({panelW, panelH});
	bg->setPosition(center);
	_mainLayer->addChild(bg, 0);

	addCorner(_mainLayer, {center.x - 178.f, center.y - 102.f}, false, false);
	addCorner(_mainLayer, {center.x - 178.f, center.y + 102.f}, false, true);
	addCorner(_mainLayer, {center.x + 178.f, center.y + 102.f}, true, true);
	addCorner(_mainLayer, {center.x + 178.f, center.y - 102.f}, true, false);

	if (auto* glowL = frameSprite("GJ_bigStar_glow_001.png"))
	{
		glowL->setPosition({center.x - 168.f, center.y + 78.f});
		glowL->setScale(0.85f);
		glowL->setOpacity(160);
		_mainLayer->addChild(glowL, 1);
	}
	if (auto* glowR = frameSprite("GJ_bigStar_glow_001.png"))
	{
		glowR->setPosition({center.x + 168.f, center.y + 78.f});
		glowR->setScale(0.85f);
		glowR->setOpacity(160);
		_mainLayer->addChild(glowR, 1);
	}

	if (auto* starL = frameSprite("GJ_bigStar_001.png"))
	{
		starL->setPosition({center.x - 168.f, center.y + 78.f});
		starL->setScale(0.55f);
		starL->runAction(RepeatForever::create(Sequence::create(
			EaseSineInOut::create(RotateBy::create(1.6f, 18.f)),
			EaseSineInOut::create(RotateBy::create(1.6f, -18.f)),
			nullptr)));
		_mainLayer->addChild(starL, 3);
	}
	if (auto* starR = frameSprite("GJ_bigStar_001.png"))
	{
		starR->setPosition({center.x + 168.f, center.y + 78.f});
		starR->setScale(0.55f);
		starR->runAction(RepeatForever::create(Sequence::create(
			EaseSineInOut::create(RotateBy::create(1.6f, -18.f)),
			EaseSineInOut::create(RotateBy::create(1.6f, 18.f)),
			nullptr)));
		_mainLayer->addChild(starR, 3);
	}

	if (auto* lock = frameSprite("GJ_lock_001.png"))
	{
		lock->setPosition({center.x, center.y + 88.f});
		lock->setScale(0.85f);
		_mainLayer->addChild(lock, 3);
	}

	if (auto* badge = frameSprite("GJ_newBtn_001.png"))
	{
		badge->setPosition({center.x + 118.f, center.y + 96.f});
		badge->setScale(0.55f);
		badge->runAction(RepeatForever::create(Sequence::create(
			EaseSineInOut::create(ScaleTo::create(0.7f, 0.62f)),
			EaseSineInOut::create(ScaleTo::create(0.7f, 0.55f)),
			nullptr)));
		_mainLayer->addChild(badge, 4);
	}

	auto* brand = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "OpenGD");
	brand->setPosition({center.x, center.y + 48.f});
	brand->setScale(0.55f);
	_mainLayer->addChild(brand, 3);

	auto* title = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "COMING SOON");
	title->setPosition({center.x, center.y + 18.f});
	title->setScale(0.82f);
	_mainLayer->addChild(title, 3);

	std::string body = "This feature will appear\nin the next version!";
	if (!featureName.empty())
		body = fmt::format("{}\nwill appear in the next version!", featureName);

	auto* desc = Label::createWithBMFont(GameToolbox::getTextureString("chatFont.fnt"), body, TextHAlignment::CENTER);
	desc->setPosition({center.x, center.y - 28.f});
	desc->setScale(1.05f);
	_mainLayer->addChild(desc, 3);

	if (auto* updateIcon = frameSprite("GJ_updateBtn_001.png"))
	{
		updateIcon->setPosition({center.x - 150.f, center.y - 88.f});
		updateIcon->setScale(0.55f);
		_mainLayer->addChild(updateIcon, 3);
	}
	if (auto* infoIcon = frameSprite("GJ_infoIcon_001.png"))
	{
		infoIcon->setPosition({center.x + 150.f, center.y - 88.f});
		infoIcon->setScale(0.7f);
		_mainLayer->addChild(infoIcon, 3);
	}

	auto* menu = Menu::create();
	menu->setPosition({center.x, center.y - 88.f});
	_mainLayer->addChild(menu, 5);

	auto* okBtn = MenuItemSpriteExtra::create(ButtonSprite::create("OK"), [this](Node*) { close(); });
	menu->addChild(okBtn);

	return true;
}

void ComingSoonLayer::show(Transitions)
{
	if (!getParent())
	{
		if (auto* scene = Director::getInstance()->getRunningScene())
		{
			int z = GameToolbox::getHighestChildZ(scene);
			if (z <= 104)
				z = 105;
			else
				++z;
			scene->addChild(this, z);
		}
	}
	setOpacity(0);
	runAction(FadeTo::create(0.14f, 150));
	if (_mainLayer)
	{
		_mainLayer->setScale(0.1f);
		_mainLayer->runAction(EaseElasticOut::create(ScaleTo::create(0.5f, 1.0f), 0.6f));
	}
}
