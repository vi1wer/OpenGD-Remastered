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

#include "LevelPage.h"
#include "MenuItemSpriteExtra.h"
#include "core/ui/UIScale9Sprite.h"
#include "PlayLayer.h"
#include <AudioEngine.h>

#include "LevelDebugLayer.h"

#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "2d/Transition.h"
#include "MenuLayer.h"
#include "GJGameLevel.h"
#include "GameManager.h"
#include "base/Director.h"
#include "GameToolbox/log.h"
#include "GameToolbox/getTextureString.h"

#include "fmt/format.h"
#include <algorithm>

bool LevelPage::replacingScene = false;

namespace
{
int orbsForStars(int stars)
{
	if (stars <= 0)
		return 0;
	static const int table[] = {0, 50, 75, 100, 125, 150, 175, 200, 225, 250, 275};
	if (stars <= 10)
		return table[stars];
	return 275 + (stars - 10) * 25;
}
} // namespace

bool LevelPage::init(GJGameLevel* level)
{
	if (!Layer::init()) return false;
	
	if (!level)
		return false;

	GameToolbox::log("normal: {}, practice: {}", level->_normalPercent, level->_practicePercent);
	
	_level = level;
	std::string barTexture = GameToolbox::getTextureString("GJ_progressBar_001.png");
	std::string bigFontTexture = GameToolbox::getTextureString("bigFont.fnt");

	const auto& winSize = ax::Director::getInstance()->getWinSize();

	auto addProgressBar = [&](float y, float percent, const char* title, const ax::Color3B& fillColor) {
		percent = std::clamp(percent, 0.f, 100.f);

		auto* bar = ax::Sprite::create(barTexture);
		if (!bar)
			return;
		bar->setPosition({winSize.width / 2.f, y});
		bar->setColor({0, 0, 0});
		bar->setOpacity(125);
		addChild(bar, 3);

		auto* fill = ax::Sprite::create(barTexture);
		if (fill)
		{
			fill->setAnchorPoint({0.f, 0.5f});
			fill->setPosition({1.36f, bar->getContentSize().height * 0.5f});
			fill->setColor(fillColor);
			fill->setTextureRect({0.f, 0.f,
				bar->getContentSize().width * (percent / 100.f),
				bar->getTextureRect().size.height});
			fill->setScaleX(0.992f);
			fill->setScaleY(0.86f);
			bar->addChild(fill);
		}

		auto* titleLabel = ax::Label::createWithBMFont(bigFontTexture, title);
		titleLabel->setPosition({winSize.width / 2.f, y + 20.f});
		titleLabel->setScale(0.55f);
		addChild(titleLabel, 4);

		auto* percLabel = ax::Label::createWithBMFont(bigFontTexture, fmt::format("{}%", static_cast<int>(percent)));
		percLabel->setPosition({winSize.width / 2.f, y});
		percLabel->enableShadow(ax::Color4B::BLACK, {0.2f, -0.2f});
		percLabel->setScale(0.55f);
		addChild(percLabel, 4);
	};

	addProgressBar(winSize.height / 2.f - 30.f, level->_normalPercent, "Normal Mode", {0, 255, 0});
	addProgressBar(winSize.height / 2.f - 80.f, level->_practicePercent, "Practice Mode", {0, 255, 255});

	auto scale9 = ax::ui::Scale9Sprite::create("square02_001.png");
	if (!scale9)
		return false;
	scale9->setContentSize({340, 95});
	scale9->setOpacity(125);

	if (auto diffIcon = ax::Sprite::createWithSpriteFrameName(GJGameLevel::getDifficultySprite(level, kMainLevels)))
	{
		diffIcon->setScale(1.1f);
		diffIcon->setPosition(35.75f, 50.5f);
		scale9->addChild(diffIcon, 0);
	}

	// Level name — full size, no shrink. Line break only for ToE / ToE 2.
	std::string displayName = level->_levelName;
	if (displayName == "Theory Of Everything")
		displayName = "Theory Of\nEverything";
	else if (displayName == "Theory Of Everything 2")
		displayName = "Theory Of\nEverything 2";

	auto levelName = ax::Label::createWithBMFont(bigFontTexture, displayName);
	if (levelName)
	{
		levelName->setAlignment(ax::TextHAlignment::CENTER, ax::TextVAlignment::CENTER);
		levelName->setAnchorPoint({0.5f, 0.5f});
		levelName->setScale(0.904f);
		levelName->setPosition(190.f, 50.5f);
		scale9->addChild(levelName, 0);
	}

	// Stars reward - top-right (original LevelPage proportions).
	if (level->_stars > 0)
	{
		auto* starIcon = ax::Sprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
		if (starIcon)
		{
			starIcon->setScale(0.85f);
			starIcon->setPosition({322.f, 78.f});
			scale9->addChild(starIcon, 1);

			auto* starAmt = ax::Label::createWithBMFont(bigFontTexture, std::to_string(level->_stars));
			starAmt->setScale(0.55f);
			starAmt->setAnchorPoint({1.f, 0.5f});
			starAmt->setPosition({starIcon->getPositionX() - 14.f, starIcon->getPositionY() + 0.5f});
			scale9->addChild(starAmt, 1);
		}
	}

	// Orbs reward - bottom-left (number left of icon, Garage/Profile style).
	const int orbReward = orbsForStars(level->_stars);
	if (orbReward > 0)
	{
		auto* orbIcon = ax::Sprite::createWithSpriteFrameName("currencyOrbIcon_001.png");
		if (orbIcon)
		{
			orbIcon->setScale(0.85f);
			orbIcon->setPosition({52.f, 20.f});
			scale9->addChild(orbIcon, 1);

			auto* orbAmt = ax::Label::createWithBMFont(bigFontTexture, std::to_string(orbReward));
			orbAmt->setScale(0.5f);
			orbAmt->setAnchorPoint({1.f, 0.5f});
			orbAmt->setPosition({orbIcon->getPositionX() - 14.f, orbIcon->getPositionY()});
			scale9->addChild(orbAmt, 1);
		}
	}

	// Secret coins - bottom-right. Original LevelPage uses secretCoinUI (large), not tiny GJ_coinsIcon.
	{
		int coinMask = 0;
		if (auto* gm = GameManager::getInstance())
			coinMask = gm->getLevelCoins(level->_levelID);

		const char* coinFrame = "secretCoinUI_001.png";
		if (!ax::SpriteFrameCache::getInstance()->getSpriteFrameByName(coinFrame))
			coinFrame = "GJ_coinsIcon_001.png";

		for (int i = 0; i < 3; i++)
		{
			auto* coin = ax::Sprite::createWithSpriteFrameName(coinFrame);
			if (!coin)
				continue;
			coin->setScale(0.4f);
			coin->setPosition({278.f + i * 24.f, 18.f});
			const bool got = (coinMask & (1 << i)) != 0;
			if (!got)
			{
				coin->setColor(ax::Color3B(70, 70, 70));
				coin->setOpacity(160);
			}
			scale9->addChild(coin, 1);
		}
	}

	auto mainBtn = MenuItemSpriteExtra::create(scale9, [this](Node* btn) { onPlay(btn); });
	mainBtn->setScaleMultiplier(1.1f);
	auto levelMenu = ax::Menu::create();
	levelMenu->addChild(mainBtn);
	levelMenu->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 60 });
	addChild(levelMenu);
	
	return true;
}

void LevelPage::onPlay(Node* btn)
{
	if (LevelPage::replacingScene)
		return;

	LevelPage::replacingScene = true;
	GJGameLevel* level = _level;
	_level = nullptr;

	ax::Scene* scene = PlayLayer::scene(level);
	ax::AudioEngine::stopAll();
	ax::AudioEngine::play2d("playSound_01.ogg", false, 0.2f);
	ax::Director::getInstance()->replaceScene(ax::TransitionFade::create(0.5f, scene));
	MenuLayer::music = false;
}

LevelPage::~LevelPage()
{
	delete _level;
	_level = nullptr;
}
LevelPage* LevelPage::create(GJGameLevel* level)
{
	LevelPage* pRet = new LevelPage();
	if (pRet->init(level))
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
