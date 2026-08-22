#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Shop/DRShopItemTable.h"
#include "MVVMViewModelBase.h"
#include "DRShopViewModel.generated.h"

class UTexture2D;

/** 상점 상품 한 건의 표시 상태다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRShopOfferEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	const FDRShopOfferView& GetOffer() const
	{
		return Offer;
	}

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	TObjectPtr<UTexture2D> ItemIcon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	FText PriceText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	bool bIsPurchasable = false;

private:
	friend class UDRShopViewModel;

	void Initialize(const FDRShopOfferView& InOffer);

	UPROPERTY(Transient)
	FDRShopOfferView Offer;
};

/** 상점 Offer를 현재 선택된 섹션의 엔트리 목록으로 제공한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRShopViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetOffers(EDRShopOfferType OfferType, const TArray<FDRShopOfferView>& NewOffers);

	/** 현재 선택된 섹션의 엔트리 목록을 위젯에 제공한다. */
	TArray<UDRShopOfferEntryViewModel*> GetOfferEntries() const;

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SelectSection(EDRShopOfferSection NewSection);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	EDRShopOfferSection SelectedSection = EDRShopOfferSection::Equipment;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Shop")
	TArray<TObjectPtr<UDRShopOfferEntryViewModel>> OfferEntries;

private:
	void RebuildOfferEntries();

	UPROPERTY(Transient)
	TArray<FDRShopOfferView> Offers;
};
