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
#include "2d/Layer.h"
#include <cstddef>
#include <string>
#include <vector>

class SimpleProgressBar;

namespace ax
{
class Scene;
class SpriteFrameCache;
class TextureCache;
} // namespace ax

class LoadingLayer : public ax::Layer
{
private:
	void buildIconQueue();
	void startIconLoading();
	void loadIconBatch(float dt);
	int getIconPlistCount(int from, int to);
	int getTotalIconPlists();

public:
	const char* getSplash();
	static ax::Scene* scene();
	static LoadingLayer* create();
	bool init();
	void loadAssets();
	void assetLoaded();
	SimpleProgressBar* _pBar = nullptr;

	float m_nAssetsLoaded = 0;
	float m_nTotalAssets = 0;

	ax::SpriteFrameCache* _sprFrameCache = nullptr;
	ax::TextureCache* _textureCache = nullptr;

	std::vector<std::string> _iconPlists;
	std::size_t _iconLoadIndex = 0;
	bool _finished = false;
};
