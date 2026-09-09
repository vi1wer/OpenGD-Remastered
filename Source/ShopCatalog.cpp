/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "ShopCatalog.h"

namespace
{
	// Official Normal shop (ShopType::Normal) store list — icons/effects only.
	// Col1/Col2/Streak need separate unlock plumbing; added when supported.
	constexpr ShopItem kShopkeeperItems[] = {
		{34, IconType::kIconTypeCube, 79, 500},
		{1, IconType::kIconTypeCube, 77, 1000},
		{2, IconType::kIconTypeCube, 86, 1000},
		{3, IconType::kIconTypeCube, 73, 1000},
		{4, IconType::kIconTypeCube, 102, 1000},
		{5, IconType::kIconTypeCube, 107, 1000},
		{6, IconType::kIconTypeShip, 27, 2000},
		{7, IconType::kIconTypeUfo, 25, 2000},
		{8, IconType::kIconTypeUfo, 23, 2500},
		{9, IconType::kIconTypeBall, 20, 1000},
		{10, IconType::kIconTypeBall, 19, 1500},
		{11, IconType::kIconTypeWave, 21, 500},
		{12, IconType::kIconTypeSpider, 2, 2000},
		{13, IconType::kIconTypeRobot, 12, 3000},
		{14, IconType::kIconTypeDeathEffect, 8, 7000},
		{15, IconType::kIconTypeDeathEffect, 11, 7000},
		{111, IconType::kIconTypeCube, 140, 3000},
		{112, IconType::kIconTypeCube, 109, 4000},
		{113, IconType::kIconTypeCube, 113, 4000},
		{114, IconType::kIconTypeBall, 40, 4000},
		{115, IconType::kIconTypeWave, 35, 4000},
	};
}

const ShopItem* ShopCatalog::items()
{
	return kShopkeeperItems;
}

int ShopCatalog::itemCount()
{
	return static_cast<int>(sizeof(kShopkeeperItems) / sizeof(kShopkeeperItems[0]));
}

const ShopItem* ShopCatalog::itemAt(int index)
{
	if (index < 0 || index >= itemCount())
		return nullptr;
	return &kShopkeeperItems[index];
}

bool ShopCatalog::isShopLockedItem(IconType type, int itemId)
{
	for (int i = 0; i < itemCount(); i++)
	{
		if (kShopkeeperItems[i].type == type && kShopkeeperItems[i].itemId == itemId)
			return true;
	}
	return false;
}
