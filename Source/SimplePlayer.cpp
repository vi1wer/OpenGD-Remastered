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

#include "GameToolbox/enums.h"
#include "SimplePlayer.h"
#include "AnimatedIconSprite.h"

#include "UTF8.h"
#include "base/Director.h"

#include "GameToolbox/math.h"
#include "GameToolbox/conv.h"
#include <algorithm>
#include <cstddef>

USING_NS_AX;

namespace
{
Sprite* tryCreateFrame(const std::string& frameName)
{
	return Sprite::createWithSpriteFrameName(frameName);
}
} // namespace

bool SimplePlayer::init(int cubeID)
{
	if (!Sprite::init())
		return false;

	this->updateGamemode(cubeID, IconType::kIconTypeCube);
	this->setStretchEnabled(false);
	this->setContentSize({60, 60});
	this->setAnchorPoint({.25f, .25f});

	return true;
}

void SimplePlayer::clearIconSprites()
{
	if (m_animSprite)
	{
		this->removeChild(m_animSprite);
		m_animSprite = nullptr;
	}

	if (!m_partMains.empty())
	{
		for (Sprite* spr : m_partMains)
		{
			if (spr)
				this->removeChild(spr);
		}
		for (Sprite* spr : m_partSeconds)
		{
			if (spr)
				this->removeChild(spr);
		}
		for (Sprite* spr : m_partGlows)
		{
			if (spr)
				this->removeChild(spr);
		}
		if (m_pExtraSprite)
			this->removeChild(m_pExtraSprite);

		m_partMains.clear();
		m_partSeconds.clear();
		m_partGlows.clear();
		m_pMainSprite = nullptr;
		m_pSecondarySprite = nullptr;
		m_pGlowSprite = nullptr;
		m_pExtraSprite = nullptr;
		m_pDomeSprite = nullptr;
		return;
	}

	auto removeSprite = [this](Sprite*& spr) {
		if (spr)
		{
			this->removeChild(spr);
			spr = nullptr;
		}
	};

	removeSprite(m_pMainSprite);
	removeSprite(m_pSecondarySprite);
	removeSprite(m_pGlowSprite);
	removeSprite(m_pExtraSprite);
	removeSprite(m_pDomeSprite);
}

void SimplePlayer::buildSimpleIcon(const char* prefix, int iconID, IconType mode)
{
	auto mainFrame = StringUtils::format("%s_%02d_001.png", prefix, iconID);
	auto secFrame = StringUtils::format("%s_%02d_2_001.png", prefix, iconID);
	auto extFrame = StringUtils::format("%s_%02d_extra_001.png", prefix, iconID);
	auto glowFrame = StringUtils::format("%s_%02d_glow_001.png", prefix, iconID);
	auto domeFrame = StringUtils::format("%s_%02d_3_001.png", prefix, iconID);

	m_pMainSprite = tryCreateFrame(mainFrame);
	if (!m_pMainSprite)
		return;
	m_pMainSprite->setAnchorPoint({0, 0});
	m_pMainSprite->setStretchEnabled(false);
	this->addChild(m_pMainSprite);

	m_pSecondarySprite = tryCreateFrame(secFrame);
	if (m_pSecondarySprite)
	{
		m_pSecondarySprite->setPosition(m_pMainSprite->getContentSize() / 2);
		m_pSecondarySprite->setStretchEnabled(false);
		this->addChild(m_pSecondarySprite, -1);
	}

	m_pGlowSprite = tryCreateFrame(glowFrame);
	if (m_pGlowSprite)
	{
		m_pGlowSprite->setPosition(m_pMainSprite->getContentSize() / 2);
		m_pGlowSprite->setStretchEnabled(false);
		m_pGlowSprite->setBlendFunc(GameToolbox::getBlending());
		m_pGlowSprite->setVisible(m_bHasGlow);
		this->addChild(m_pGlowSprite, -2);
	}

	m_pExtraSprite = tryCreateFrame(extFrame);
	if (m_pExtraSprite)
	{
		m_pExtraSprite->setPosition(m_pMainSprite->getContentSize() / 2);
		m_pExtraSprite->setStretchEnabled(false);
		this->addChild(m_pExtraSprite);
	}

	if (mode == IconType::kIconTypeUfo)
	{
		m_pDomeSprite = tryCreateFrame(domeFrame);
		if (m_pDomeSprite)
		{
			m_pDomeSprite->setPosition(m_pMainSprite->getContentSize() / 2);
			m_pDomeSprite->setStretchEnabled(false);
			this->addChild(m_pDomeSprite, -1);
		}
	}
}

void SimplePlayer::buildAnimatedIcon(const char* prefix, int iconID, IconType mode)
{
	(void)prefix;
	m_animSprite = AnimatedIconSprite::create(mode, iconID);
	if (!m_animSprite)
		return;

	m_animSprite->setPosition({0.f, 0.f});
	m_animSprite->setGlow(m_bHasGlow);
	m_animSprite->setMainColor(m_MainColor);
	m_animSprite->setSecondaryColor(m_SecondaryColor);
	m_animSprite->setGlowColor(m_GlowColor);
	this->addChild(m_animSprite, 1);

	if (m_playIdleAnimation)
		m_animSprite->playAnimation("idle01", true, true);
	else
		m_animSprite->playAnimation("idle", false, true);

	// Animation plist poses are authored around an arbitrary origin — nudge so
	// the icon sits in the same 60x60 garage cell as cube/ship icons.
	const Rect animBounds = m_animSprite->computeBounds();
	if (animBounds.size.width > 0.f && animBounds.size.height > 0.f)
	{
		const Vec2 cellCenter{30.f, 30.f};
		m_animSprite->setPosition({
			cellCenter.x - animBounds.getMidX(),
			cellCenter.y - animBounds.getMidY()
		});
	}
}

void SimplePlayer::updateGamemode(int iconID, IconType mode)
{
	int maxIcon = GameToolbox::getValueForGamemode(mode);
	if (maxIcon < 1)
		maxIcon = 1;
	iconID = GameToolbox::inRange(iconID, 1, maxIcon);

	auto tipo = GameToolbox::getNameGamemode(mode);
	clearIconSprites();

	if (mode == IconType::kIconTypeRobot || mode == IconType::kIconTypeSpider)
		buildAnimatedIcon(tipo, iconID, mode);
	else
		buildSimpleIcon(tipo, iconID, mode);

	this->updateIconColors();
}

void SimplePlayer::updateIconColors()
{
	if (m_pMainSprite)
		m_pMainSprite->setColor(m_MainColor);
	if (m_pSecondarySprite)
		m_pSecondarySprite->setColor(m_SecondaryColor);
	if (m_pGlowSprite)
		m_pGlowSprite->setColor(m_GlowColor);

	if (m_animSprite)
	{
		m_animSprite->setMainColor(m_MainColor);
		m_animSprite->setSecondaryColor(m_SecondaryColor);
		m_animSprite->setGlowColor(m_GlowColor);
	}

	for (Sprite* spr : m_partMains)
	{
		if (spr)
			spr->setColor(m_MainColor);
	}
	for (Sprite* spr : m_partSeconds)
	{
		if (spr)
			spr->setColor(m_SecondaryColor);
	}
	for (Sprite* spr : m_partGlows)
	{
		if (spr)
			spr->setColor(m_GlowColor);
	}
}

void SimplePlayer::setMainColor(Color3B col)
{
	m_MainColor = col;
	updateIconColors();
}

void SimplePlayer::setSecondaryColor(Color3B col)
{
	m_SecondaryColor = col;
	updateIconColors();
}

void SimplePlayer::setGlow(bool glow)
{
	m_bHasGlow = glow;
	if (m_pGlowSprite)
		m_pGlowSprite->setVisible(glow);
	if (m_animSprite)
		m_animSprite->setGlow(glow);
	for (Sprite* spr : m_partGlows)
	{
		if (spr)
			spr->setVisible(glow);
	}
}

void SimplePlayer::setGlowColor(Color3B col)
{
	m_GlowColor = col;
	updateIconColors();
}

void SimplePlayer::setPlayIdleAnimation(bool play)
{
	m_playIdleAnimation = play;
	if (!m_animSprite)
		return;
	if (play)
		m_animSprite->playAnimation("idle01", true, true);
	else
		m_animSprite->playAnimation("idle", false, true);
}

ax::Rect SimplePlayer::computeIconBounds() const
{
	Rect bounds;
	bool hasBounds = false;

	auto includeSprite = [&](Sprite* spr) {
		if (!spr || !spr->isVisible())
			return;
		const Rect box = spr->getBoundingBox();
		if (box.size.width <= 0.f && box.size.height <= 0.f)
			return;
		if (!hasBounds)
		{
			bounds = box;
			hasBounds = true;
		}
		else
		{
			const float minX = std::min(bounds.getMinX(), box.getMinX());
			const float minY = std::min(bounds.getMinY(), box.getMinY());
			const float maxX = std::max(bounds.getMaxX(), box.getMaxX());
			const float maxY = std::max(bounds.getMaxY(), box.getMaxY());
			bounds = Rect(minX, minY, maxX - minX, maxY - minY);
		}
	};

	includeSprite(m_pMainSprite);
	includeSprite(m_pSecondarySprite);
	includeSprite(m_pExtraSprite);
	includeSprite(m_pDomeSprite);
	if (m_animSprite)
	{
		const Rect local = m_animSprite->computeBounds();
		const Rect box(local.origin + m_animSprite->getPosition(), local.size);
		if (!hasBounds)
		{
			bounds = box;
			hasBounds = true;
		}
		else
		{
			const float minX = std::min(bounds.getMinX(), box.getMinX());
			const float minY = std::min(bounds.getMinY(), box.getMinY());
			const float maxX = std::max(bounds.getMaxX(), box.getMaxX());
			const float maxY = std::max(bounds.getMaxY(), box.getMaxY());
			bounds = Rect(minX, minY, maxX - minX, maxY - minY);
		}
	}
	for (Sprite* spr : m_partMains)
		includeSprite(spr);
	for (Sprite* spr : m_partSeconds)
		includeSprite(spr);

	if (!hasBounds)
		return Rect(0.f, 0.f, 30.f, 30.f);
	return bounds;
}

void SimplePlayer::fitToSize(float targetSize)
{
	setStretchEnabled(false);
	setScale(1.f);
	const Rect bounds = computeIconBounds();
	const float maxDim = std::max(bounds.size.width, bounds.size.height);
	if (maxDim > 0.01f)
		setScale(targetSize / maxDim);
}

void SimplePlayer::placeCenteredAt(const Vec2& parentPoint)
{
	// Anchor at local origin so position + scaled AABB center == parentPoint.
	setAnchorPoint({0.f, 0.f});
	const Rect bounds = computeIconBounds();
	setPosition({
		parentPoint.x - bounds.getMidX() * getScaleX(),
		parentPoint.y - bounds.getMidY() * getScaleY()
	});
}

void SimplePlayer::applyGarageStyle(float targetSize)
{
	setGlow(false);
	setMainColor({170, 170, 170});
	setSecondaryColor({95, 95, 95});
	fitToSize(targetSize);
}

SimplePlayer* SimplePlayer::create(int cubeID)
{
	auto pRet = new (std::nothrow) SimplePlayer();

	if (pRet && pRet->init(cubeID))
	{
		pRet->autorelease();
		return pRet;
	}
	AX_SAFE_DELETE(pRet);
	return pRet;
}
