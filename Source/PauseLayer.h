/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "2d/Layer.h"

class PlayLayer;

class PauseLayer : public ax::LayerColor
{
public:
	static PauseLayer* create(PlayLayer* playLayer);
	bool init(PlayLayer* playLayer);

private:
	PlayLayer* _playLayer = nullptr;

	void onResume(ax::Node*);
	void onRestart(ax::Node*);
	void onExitLevel(ax::Node*);
	void onPractice(ax::Node*);
	void onSettings(ax::Node*);
	void showSoon(const char* featureName);
};
