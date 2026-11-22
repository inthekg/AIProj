#include "ShopTypes.h"

bool IsBaseIngredient(EIngredient Ingredient)
{
	switch (Ingredient)
	{
	case EIngredient::RedBean:
	case EIngredient::Yogurt:
	case EIngredient::DubaiChoco:
		return true;
	case EIngredient::Strawberry:
	case EIngredient::Blueberry:
	case EIngredient::Pineapple:
	case EIngredient::Mango:
		return false;
	case EIngredient::None:
		break;
	}
	checkNoEntry();
	return false;
}
