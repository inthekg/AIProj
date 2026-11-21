#include "ShopTypes.h"

bool IsBaseIngredient(EIngredient Ingredient)
{
	// 베이스 · 과일 구분의 원본은 이 switch다(CSV에 두지 않음 — data-schema.md). Scripts/balance_sim.py의 BASES와 같게 둔다
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
	// Shipping에서는 checkNoEntry가 빈 매크로라(AssertionMacros.h) 반환이 필요하다
	return false;
}
