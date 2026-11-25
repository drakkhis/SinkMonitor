#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "SinkMonitorTypes.h"
#include "UI/FGInteractWidget.h"
#include "SinkMonitorInteractWidget.generated.h"

class ASinkMonitorSubsystem;
class UTexture2D;

#if !UE_SERVER
class SBorder;
class SButton;
class SCheckBox;
class SEditableTextBox;
class SImage;
class SProgressBar;
class SScrollBox;
class STextBlock;
struct FSlateBrush;
#endif

/**
 * Pure-C++ Slate interactive terminal widget opened when interacting with ASinkMonitorBuildable.
 * Provides live searchable, filterable, sortable item telemetry, pinning, and active sink details.
 */
UCLASS()
class SINKMONITOR_API USinkMonitorInteractWidget : public UFGInteractWidget
{
	GENERATED_BODY()

public:
	USinkMonitorInteractWidget(const FObjectInitializer& ObjectInitializer);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
#if !UE_SERVER
	struct FSlateBrushes;
	struct FItemRowWidgets;

	void RefreshData();
	void RebuildOrUpdateRows(const TArray<FSinkableItemEntry>& FilteredItems);
	bool DoesRowOrderMatch(const TArray<FSinkableItemEntry>& FilteredItems) const;
	void UpdateRowVisuals(FItemRowWidgets& Row, const FSinkableItemEntry& Entry, bool bSelected);
	void UpdateDetailPanel(const TArray<FSinkableItemEntry>& AllItems);
	void UpdateFilterAndSortVisuals();

	void HandleSearchTextChanged(const FText& NewText);
	void HandleActiveOnlyChanged(ECheckBoxState NewState);
	FReply HandleClearSearchClicked();
	FReply HandleSetTrackFilter(ESinkMonitorTrackFilter NewFilter);
	FReply HandleSetSortMode(ESinkMonitorSortMode NewMode);
	FReply HandleRowClicked(TSubclassOf<UFGItemDescriptor> ClickedItemClass);
	FReply HandlePinSelectedClicked();
	FReply HandleResetSessionClicked();
	FReply HandleCloseClicked();

	ASinkMonitorSubsystem* ResolveSubsystem();

	TSharedPtr<FSlateBrushes> Brushes;
	TSharedPtr<SEditableTextBox> SearchTextBox;
	TSharedPtr<SCheckBox> ActiveOnlyCheckBox;
	TSharedPtr<SScrollBox> ItemListScrollBox;
	TSharedPtr<SScrollBox> ActiveSinksScrollBox;

	TSharedPtr<STextBlock> HeaderSummaryText;
	TSharedPtr<STextBlock> StandardCouponPtsText;
	TSharedPtr<STextBlock> StandardCouponMetaText;
	TSharedPtr<SProgressBar> StandardCouponProgressBar;
	TSharedPtr<STextBlock> DnaCouponPtsText;
	TSharedPtr<STextBlock> DnaCouponMetaText;
	TSharedPtr<SProgressBar> DnaCouponProgressBar;
	TSharedPtr<STextBlock> CouponsReadyText;
	TSharedPtr<STextBlock> ItemCountStatusText;

	TSharedPtr<SBorder> TrackBtnBorders[3];
	TSharedPtr<STextBlock> TrackBtnTexts[3];
	TSharedPtr<SBorder> SortBtnBorders[5];
	TSharedPtr<STextBlock> SortBtnTexts[5];

	TSharedPtr<SImage> DetailIconImage;
	TSharedPtr<FSlateBrush> DetailIconBrush;
	TSharedPtr<STextBlock> DetailItemNameText;
	TSharedPtr<SBorder> DetailTrackBadgeBorder;
	TSharedPtr<STextBlock> DetailTrackBadgeText;
	TSharedPtr<SBorder> PinButtonBorder;
	TSharedPtr<STextBlock> PinButtonText;

	TSharedPtr<STextBlock> DetailPointValueText;
	TSharedPtr<STextBlock> DetailRatePerMinText;
	TSharedPtr<STextBlock> DetailPointsPerMinText;
	TSharedPtr<STextBlock> DetailSessionTotalText;
	TSharedPtr<STextBlock> DetailLifetimeTotalText;
	TSharedPtr<STextBlock> DetailActiveSinksHeaderText;

	TArray<FItemRowWidgets> RowWidgets;
	FString CurrentSearchQuery;
	bool bShowActiveOnly = false;
	ESinkMonitorTrackFilter TrackFilter = ESinkMonitorTrackFilter::All;
	ESinkMonitorSortMode SortMode = ESinkMonitorSortMode::RateDesc;
#endif // !UE_SERVER

	UFUNCTION()
	void HandleSubsystemStatsUpdated();

	UPROPERTY(Transient)
	TSubclassOf<UFGItemDescriptor> SelectedItemClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> CachedIconTextures;

	TWeakObjectPtr<ASinkMonitorSubsystem> BoundSubsystem;
	FTimerHandle RefreshTimerHandle;
};
