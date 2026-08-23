/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "2d/Node.h"
#include "GameToolbox/enums.h"
#include "Types.h"
#include "math/Rect.h"
#include "math/Vec2.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ax
{
class Sprite;
}

class AnimatedIconSprite : public ax::Node
{
  public:
	static AnimatedIconSprite* create(IconType type, int iconId);

	bool init(IconType type, int iconId);

	void setIconId(int iconId);
	void setMainColor(const ax::Color3B& color);
	void setSecondaryColor(const ax::Color3B& color);
	void setGlowColor(const ax::Color3B& color);
	void setGlow(bool glow);

	void playAnimation(const std::string& name, bool loop = true, bool restart = false);
	void stopAnimation();
	const std::string& currentAnimation() const { return _currentAnim; }
	bool hasAnimation(const std::string& name) const;

	ax::Rect computeBounds() const;

	void update(float dt) override;

  private:
	struct PartPose
	{
		ax::Vec2 position{0.f, 0.f};
		ax::Vec2 scale{1.f, 1.f};
		ax::Vec2 flipped{0.f, 0.f};
		float rotation = 0.f;
		int z = 0;
		bool valid = false;
	};

	struct AnimFrame
	{
		std::vector<PartPose> parts;
	};

	struct Animation
	{
		std::vector<AnimFrame> frames;
	};

	struct PartDef
	{
		int tag = 0;
		int partIndex = 1;
	};

	struct AnimData
	{
		std::vector<PartDef> parts;
		std::unordered_map<std::string, Animation> animations;
	};

	struct PartSprites
	{
		ax::Sprite* main = nullptr;
		ax::Sprite* second = nullptr;
		ax::Sprite* glow = nullptr;
		ax::Sprite* extra = nullptr;
	};

	static const AnimData* loadAnimData(IconType type);

	void rebuildParts();
	void applyFrame(const AnimFrame& frame);
	void applySpritePose(ax::Sprite* spr, const PartPose& pose, int extraZ);
	std::string resolveAnimationName(const std::string& name) const;
	void finishNonLooping();

	IconType _type = IconType::kIconTypeRobot;
	int _iconId = 1;
	bool _hasGlow = false;
	ax::Color3B _mainColor{255, 255, 255};
	ax::Color3B _secondColor{255, 255, 255};
	ax::Color3B _glowColor{255, 255, 255};

	const AnimData* _data = nullptr;
	std::vector<PartSprites> _parts;

	std::string _currentAnim;
	int _frameIndex = 0;
	float _frameTime = 0.f;
	float _frameDuration = 1.f / 30.f;
	bool _loop = true;
	bool _playing = false;
};
