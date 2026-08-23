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

#include "2d/Sprite.h"
#include "Types.h"

#include "GameToolbox/enums.h"
#include <vector>

class AnimatedIconSprite;

class SimplePlayer : public ax::Sprite {
public:
	bool init(int cubeID);

	ax::Sprite* m_pMainSprite = nullptr;
	ax::Sprite* m_pSecondarySprite = nullptr;
	ax::Sprite* m_pGlowSprite = nullptr;
	ax::Sprite* m_pExtraSprite = nullptr;
	ax::Sprite* m_pDomeSprite = nullptr;

	std::vector<ax::Sprite*> m_partMains;
	std::vector<ax::Sprite*> m_partSeconds;
	std::vector<ax::Sprite*> m_partGlows;
	AnimatedIconSprite* m_animSprite = nullptr;

	bool m_bHasGlow = false;
	bool m_playIdleAnimation = false;

	ax::Color3B m_MainColor = {255, 255, 255};
	ax::Color3B m_SecondaryColor = {255, 255, 255};
	ax::Color3B m_GlowColor = {255, 255, 255};

public:
	static SimplePlayer* create(int cubeID);

	void updateGamemode(int iconID, IconType mode);
	void setMainColor(ax::Color3B col);
	void setSecondaryColor(ax::Color3B col);
	void setGlowColor(ax::Color3B col);
	void updateIconColors();
	void setGlow(bool glow);
	void setPlayIdleAnimation(bool play);

	// Neutral gray look + uniform bounding size for garage grids.
	void applyGarageStyle(float targetSize = 26.f);
	void fitToSize(float targetSize);
	void placeCenteredAt(const ax::Vec2& parentPoint);

private:
	void clearIconSprites();
	void buildSimpleIcon(const char* prefix, int iconID, IconType mode);
	void buildAnimatedIcon(const char* prefix, int iconID, IconType mode);
	ax::Rect computeIconBounds() const;
};
