/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "GameToolbox/enums.h"
#include <cstddef>

struct ShopItem
{
	int listingId;
	IconType type;
	int itemId;
	int costOrbs;
};

namespace ShopCatalog
{
	const ShopItem* items();
	int itemCount();
	const ShopItem* itemAt(int index);
	bool isShopLockedItem(IconType type, int itemId);
}
