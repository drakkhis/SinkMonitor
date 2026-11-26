#include "UI/SinkMonitorInteractWidget.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FGResourceSinkSubsystem.h"
#include "GameFramework/Actor.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Resources/FGItemDescriptor.h"
#include "SinkMonitorSubsystem.h"
#include "TimerManager.h"

#if !UE_SERVER
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#endif

#if !UE_SERVER
namespace
{
const FLinearColor BackdropDimColor(0.01f, 0.015f, 0.02f, 0.72f);
const FLinearColor CardBgColor(0.055f, 0.068f, 0.09f, 0.97f);
const FLinearColor PanelBgColor(0.085f, 0.105f, 0.138f, 0.96f);
const FLinearColor RowBgEvenColor(0.072f, 0.088f, 0.115f, 0.92f);
const FLinearColor RowBgOddColor(0.085f, 0.104f, 0.135f, 0.92f);
const FLinearColor RowSelectedBgColor(0.24f, 0.145f, 0.055f, 0.96f);
const FLinearColor FicsitOrangeColor(0.98f, 0.58f, 0.14f, 1.0f);
const FLinearColor CyanAccentColor(0.20f, 0.84f, 0.96f, 1.0f);
const FLinearColor ActiveMintColor(0.18f, 0.90f, 0.62f, 1.0f);
const FLinearColor PrimaryTextColor(0.94f, 0.96f, 0.98f, 1.0f);
const FLinearColor MutedTextColor(0.58f, 0.64f, 0.72f, 1.0f);
const FLinearColor InactiveBtnColor(0.12f, 0.15f, 0.20f, 0.95f);
const FLinearColor ActiveBtnColor(0.88f, 0.46f, 0.10f, 0.96f);
const FLinearColor DangerBtnColor(0.32f, 0.10f, 0.10f, 0.95f);
const FLinearColor DividerColor(0.16f, 0.21f, 0.28f, 1.0f);

FLinearColor MakeTintBg(const FLinearColor& Accent, float Factor = 0.24f)
{
	return FLinearColor(Accent.R * Factor, Accent.G * Factor, Accent.B * Factor, 0.95f);
}

FString FormatIntWithCommas(int64 Value)
{
	const bool bNegative = Value < 0;
	uint64 AbsVal = bNegative ? static_cast<uint64>(-Value) : static_cast<uint64>(Value);
	FString Digits = FString::Printf(TEXT("%llu"), AbsVal);

	FString Out;
	Out.Reserve(Digits.Len() + Digits.Len() / 3);
	int32 Count = 0;
	for (int32 i = Digits.Len() - 1; i >= 0; --i)
	{
		if (Count > 0 && (Count % 3) == 0)
		{
			Out.InsertAt(0, TEXT(','));
		}
		Out.InsertAt(0, Digits[i]);
		++Count;
	}
	if (bNegative)
	{
		Out.InsertAt(0, TEXT('-'));
	}
	return Out;
}

FString FormatRatePerMin(float Rate)
{
	if (FMath::Abs(Rate - FMath::RoundToFloat(Rate)) < 0.05f)
	{
		return FString::Printf(TEXT("%s / min"), *FormatIntWithCommas(FMath::RoundToInt64(Rate)));
	}
	return FString::Printf(TEXT("%.1f / min"), Rate);
}

FString FormatPointsPerMin(float PointsPerMin)
{
	return FString::Printf(TEXT("%s pts/min"), *FormatIntWithCommas(FMath::RoundToInt64(PointsPerMin)));
}

FString FormatCouponEta(int64 PointsRemaining, float PointsPerMin)
{
	if (PointsRemaining <= 0)
	{
		return TEXT("Ready");
	}
	if (PointsPerMin <= 0.01f)
	{
		return TEXT("Idle");
	}

	const double TotalSeconds = (static_cast<double>(PointsRemaining) / static_cast<double>(PointsPerMin)) * 60.0;
	if (TotalSeconds < 60.0)
	{
		return FString::Printf(TEXT("~%ds"), FMath::Max(1, FMath::RoundToInt(TotalSeconds)));
	}
	if (TotalSeconds < 3600.0)
	{
		const int32 WholeSec = FMath::RoundToInt(TotalSeconds);
		return FString::Printf(TEXT("~%dm %02ds"), WholeSec / 60, WholeSec % 60);
	}
	if (TotalSeconds < 86400.0)
	{
		const int32 WholeMin = FMath::RoundToInt(TotalSeconds / 60.0);
		return FString::Printf(TEXT("~%dh %02dm"), WholeMin / 60, WholeMin % 60);
	}
	return TEXT(">24h");
}

void UpdateCouponTrackWidgets(
	AFGResourceSinkSubsystem* SinkSub,
	EResourceSinkTrack Track,
	float TrackPtsPerMin,
	const TSharedPtr<STextBlock>& PtsText,
	const TSharedPtr<STextBlock>& MetaText,
	const TSharedPtr<SProgressBar>& ProgressBar)
{
	if (!SinkSub)
	{
		if (PtsText.IsValid())
		{
			PtsText->SetText(FText::FromString(TEXT("-- pts")));
		}
		if (MetaText.IsValid())
		{
			MetaText->SetText(FText::FromString(TEXT("0%  •  --")));
		}
		if (ProgressBar.IsValid())
		{
			ProgressBar->SetPercent(TOptional<float>(0.0f));
		}
		return;
	}

	const int64 PointsLeft = SinkSub->GetNumPointsToNextCoupon(Track);
	const float Fraction = FMath::Clamp(SinkSub->GetProgressionTowardsNextCoupon(Track), 0.0f, 1.0f);
	const FString EtaStr = FormatCouponEta(PointsLeft, TrackPtsPerMin);

	if (PtsText.IsValid())
	{
		PtsText->SetText(FText::FromString(FString::Printf(
			TEXT("%s pts"),
			*FormatIntWithCommas(PointsLeft))));
	}
	if (MetaText.IsValid())
	{
		MetaText->SetText(FText::FromString(FString::Printf(
			TEXT("%.1f%%  •  %s"),
			Fraction * 100.0f,
			*EtaStr)));
	}
	if (ProgressBar.IsValid())
	{
		ProgressBar->SetPercent(TOptional<float>(Fraction));
	}
}

void SyncBrushToTexture(
	const TSharedPtr<FSlateBrush>& Brush,
	const TSharedPtr<SImage>& ImageWidget,
	UTexture2D* Texture,
	float IconSize,
	TArray<TObjectPtr<UTexture2D>>& OutCachedTextures)
{
	if (!Brush.IsValid() || !ImageWidget.IsValid())
	{
		return;
	}

	if (Texture)
	{
		OutCachedTextures.AddUnique(Texture);
		Brush->SetResourceObject(Texture);
		Brush->DrawAs = ESlateBrushDrawType::Image;
		Brush->ImageType = ESlateBrushImageType::FullColor;
		Brush->SetImageSize(FVector2f(IconSize, IconSize));
		Brush->TintColor = FSlateColor(FLinearColor::White);
		ImageWidget->SetVisibility(EVisibility::HitTestInvisible);
	}
	else
	{
		Brush->SetResourceObject(nullptr);
		Brush->DrawAs = ESlateBrushDrawType::NoDrawType;
		ImageWidget->SetVisibility(EVisibility::Hidden);
	}
}
} // namespace

struct USinkMonitorInteractWidget::FSlateBrushes
{
	TSharedPtr<FSlateBrush> WhiteBrush;
	TSharedPtr<FSlateBrush> CardBrush;
	TSharedPtr<FSlateBrush> PanelBrush;
	TSharedPtr<FSlateBrush> RowBrush;
	TSharedPtr<FSlateBrush> PillBrush;
	FButtonStyle TransparentButtonStyle;
	FProgressBarStyle ProgressBarStyle;

	void Initialize()
	{
		WhiteBrush = MakeShared<FSlateColorBrush>(FLinearColor::White);
		CardBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 8.0f);
		PanelBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 5.0f);
		RowBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 4.0f);
		PillBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 4.0f);

		FSlateBrush EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		TransparentButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		TransparentButtonStyle.SetNormal(EmptyBrush);
		TransparentButtonStyle.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.06f), 4.0f));
		TransparentButtonStyle.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), 4.0f));
		TransparentButtonStyle.SetNormalPadding(FMargin(0.0f));
		TransparentButtonStyle.SetPressedPadding(FMargin(0.0f));

		ProgressBarStyle = FProgressBarStyle();
		ProgressBarStyle.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.04f, 0.052f, 0.07f, 1.0f), 3.0f));
		ProgressBarStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.0f));
		ProgressBarStyle.SetEnableFillAnimation(false);
	}

	void Reset()
	{
		PillBrush.Reset();
		RowBrush.Reset();
		PanelBrush.Reset();
		CardBrush.Reset();
		WhiteBrush.Reset();
	}
};

struct USinkMonitorInteractWidget::FItemRowWidgets
{
	TSubclassOf<UFGItemDescriptor> ItemClass;
	int32 RowIndex = 0;
	TSharedPtr<SBorder> RowBorder;
	TSharedPtr<FSlateBrush> IconBrush;
	TSharedPtr<SImage> IconImage;
	TSharedPtr<STextBlock> PinText;
	TSharedPtr<STextBlock> NameText;
	TSharedPtr<SBorder> TrackTagBorder;
	TSharedPtr<STextBlock> TrackTagText;
	TSharedPtr<STextBlock> PointValueText;
	TSharedPtr<STextBlock> RateText;
	TSharedPtr<STextBlock> PointsPerMinText;
};
#endif // !UE_SERVER

USinkMonitorInteractWidget::USinkMonitorInteractWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	mUseKeyboard = true;
	mUseMouse = true;
	mCaptureInput = true;
	mMouseLockMode = EMouseLockMode::DoNotLock;
	mDisablePlayerActions = true;
	mDisableBuildGunActions = true;
	mDisablePlayerEquipmentManagement = true;
	mDisableVisualizationModeActions = true;
	mRestoreFocusWhenLost = false;
	mSupportsStacking = false;
	mSupportsCaching = false;
	mDesiredHorizontalAlignment = HAlign_Fill;
	mDesiredVerticalAlignment = VAlign_Fill;
	mDesiredAlignmentSize = FSlateChildSize(ESlateSizeRule::Fill);
}

void USinkMonitorInteractWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

#if !UE_SERVER
	RowWidgets.Reset();
	CachedIconTextures.Reset();

	SearchTextBox.Reset();
	ActiveOnlyCheckBox.Reset();
	ItemListScrollBox.Reset();
	ActiveSinksScrollBox.Reset();
	HeaderSummaryText.Reset();
	StandardCouponPtsText.Reset();
	StandardCouponMetaText.Reset();
	StandardCouponProgressBar.Reset();
	DnaCouponPtsText.Reset();
	DnaCouponMetaText.Reset();
	DnaCouponProgressBar.Reset();
	CouponsReadyText.Reset();
	ItemCountStatusText.Reset();

	for (int32 i = 0; i < 3; ++i)
	{
		TrackBtnBorders[i].Reset();
		TrackBtnTexts[i].Reset();
	}
	for (int32 i = 0; i < 5; ++i)
	{
		SortBtnBorders[i].Reset();
		SortBtnTexts[i].Reset();
	}

	DetailIconImage.Reset();
	DetailIconBrush.Reset();
	DetailItemNameText.Reset();
	DetailTrackBadgeBorder.Reset();
	DetailTrackBadgeText.Reset();
	PinButtonBorder.Reset();
	PinButtonText.Reset();
	DetailPointValueText.Reset();
	DetailRatePerMinText.Reset();
	DetailPointsPerMinText.Reset();
	DetailSessionTotalText.Reset();
	DetailLifetimeTotalText.Reset();
	DetailActiveSinksHeaderText.Reset();

	if (Brushes.IsValid())
	{
		Brushes->Reset();
		Brushes.Reset();
	}
#endif
}

TSharedRef<SWidget> USinkMonitorInteractWidget::RebuildWidget()
{
	TSharedRef<SWidget> BaseWidget = Super::RebuildWidget();
#if UE_SERVER
	return BaseWidget;
#else
	if (!Brushes.IsValid())
	{
		Brushes = MakeShared<FSlateBrushes>();
		Brushes->Initialize();
	}

	RowWidgets.Reset();
	CachedIconTextures.Reset();
	DetailIconBrush = MakeShared<FSlateBrush>();
	DetailIconBrush->DrawAs = ESlateBrushDrawType::NoDrawType;

	auto MakeToolbarButton = [&](
		const FText& Label,
		TSharedPtr<SBorder>& OutBorder,
		TSharedPtr<STextBlock>& OutText,
		FOnClicked OnClickedDelegate,
		const FLinearColor& DefaultBg = InactiveBtnColor) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&Brushes->TransparentButtonStyle)
			.OnClicked(OnClickedDelegate)
			[
				SAssignNew(OutBorder, SBorder)
				.BorderImage(Brushes->PillBrush.Get())
				.BorderBackgroundColor(DefaultBg)
				.Padding(FMargin(10.0f, 5.0f))
				[
					SAssignNew(OutText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(PrimaryTextColor)
					.Text(Label)
				]
			];
	};

	auto MakeDetailMetricRow = [&](const FText& Label, TSharedPtr<STextBlock>& OutValueText, const FLinearColor& ValueColor) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.BorderImage(Brushes->RowBrush.Get())
			.BorderBackgroundColor(RowBgEvenColor)
			.Padding(FMargin(12.0f, 8.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(MutedTextColor)
					.Text(Label)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(OutValueText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(ValueColor)
					.Text(FText::FromString(TEXT("0")))
				]
			];
	};

	auto MakeCouponTrackCard = [&](
		const FText& TrackLabel,
		const FLinearColor& AccentColor,
		TSharedPtr<STextBlock>& OutPtsText,
		TSharedPtr<STextBlock>& OutMetaText,
		TSharedPtr<SProgressBar>& OutProgressBar) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.BorderImage(Brushes->PanelBrush.Get())
			.BorderBackgroundColor(PanelBgColor)
			.Padding(FMargin(12.0f, 8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.ColorAndOpacity(AccentColor)
						.Text(TrackLabel)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.0f, 0.0f, 8.0f, 0.0f))
					[
						SAssignNew(OutPtsText, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(PrimaryTextColor)
						.Text(FText::FromString(TEXT("-- pts")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(OutMetaText, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(MutedTextColor)
						.Text(FText::FromString(TEXT("0.0%  •  Idle")))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(7.0f)
					[
						SAssignNew(OutProgressBar, SProgressBar)
						.Style(&Brushes->ProgressBarStyle)
						.BarFillType(EProgressBarFillType::LeftToRight)
						.FillColorAndOpacity(AccentColor)
						.BorderPadding(FVector2D::ZeroVector)
						.Percent(TOptional<float>(0.0f))
					]
				]
			];
	};

	TSharedPtr<SBorder> ResetBtnBorder;
	TSharedPtr<STextBlock> ResetBtnText;
	TSharedPtr<SBorder> CloseBtnBorder;
	TSharedPtr<STextBlock> CloseBtnText;
	TSharedPtr<SBorder> ClearSearchBorder;
	TSharedPtr<STextBlock> ClearSearchText;

	return SNew(SOverlay)
		// Dimmed full-screen background
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(Brushes->WhiteBrush.Get())
			.BorderBackgroundColor(BackdropDimColor)
		]
		// Centered terminal card
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(1180.0f)
			.HeightOverride(760.0f)
			[
				SNew(SBorder)
				.BorderImage(Brushes->CardBrush.Get())
				.BorderBackgroundColor(CardBgColor)
				.Padding(0.0f)
				[
					SNew(SVerticalBox)
					// Top FICSIT accent bar
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(Brushes->WhiteBrush.Get())
						.BorderBackgroundColor(FicsitOrangeColor)
						.Padding(0.0f)
						[
							SNew(SBox)
							.HeightOverride(4.0f)
						]
					]
					// Header Bar
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(20.0f, 14.0f, 20.0f, 12.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
								.ColorAndOpacity(FicsitOrangeColor)
								.Text(NSLOCTEXT("SinkMonitor", "TerminalHeaderSub", "FICSIT INC. // AWESOME SINK TELEMETRY"))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
								.ColorAndOpacity(PrimaryTextColor)
								.Text(NSLOCTEXT("SinkMonitor", "TerminalHeaderTitle", "AWESOME Sink Monitor Terminal"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.0f, 0.0f, 16.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(Brushes->PanelBrush.Get())
							.BorderBackgroundColor(PanelBgColor)
							.Padding(FMargin(14.0f, 8.0f))
							[
								SAssignNew(HeaderSummaryText, STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
								.ColorAndOpacity(ActiveMintColor)
								.Text(FText::FromString(TEXT("0 pts/min  •  0 Active Items  •  0 Sinks")))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
						[
							MakeToolbarButton(
								NSLOCTEXT("SinkMonitor", "ResetSessionBtn", "Reset Session"),
								ResetBtnBorder,
								ResetBtnText,
								FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleResetSessionClicked),
								DangerBtnColor)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							MakeToolbarButton(
								NSLOCTEXT("SinkMonitor", "CloseBtn", "Close [ESC]"),
								CloseBtnBorder,
								CloseBtnText,
								FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleCloseClicked),
								InactiveBtnColor)
						]
					]
					// Divider
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(Brushes->WhiteBrush.Get())
						.BorderBackgroundColor(DividerColor)
						.Padding(0.0f)
						[
							SNew(SBox)
							.HeightOverride(1.0f)
						]
					]
					// Coupon Status Strip
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(20.0f, 10.0f, 20.0f, 10.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							MakeCouponTrackCard(
								NSLOCTEXT("SinkMonitor", "StandardCouponTitle", "STANDARD TRACK • NEXT COUPON"),
								FicsitOrangeColor,
								StandardCouponPtsText,
								StandardCouponMetaText,
								StandardCouponProgressBar)
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
						[
							MakeCouponTrackCard(
								NSLOCTEXT("SinkMonitor", "DnaCouponTitle", "DNA TRACK • NEXT COUPON"),
								CyanAccentColor,
								DnaCouponPtsText,
								DnaCouponMetaText,
								DnaCouponProgressBar)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Fill)
						[
							SNew(SBox)
							.MinDesiredWidth(132.0f)
							[
								SNew(SBorder)
								.BorderImage(Brushes->PanelBrush.Get())
								.BorderBackgroundColor(MakeTintBg(FicsitOrangeColor, 0.22f))
								.Padding(FMargin(14.0f, 6.0f))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.AutoHeight()
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
										.ColorAndOpacity(FicsitOrangeColor)
										.Text(NSLOCTEXT("SinkMonitor", "CouponsReadyTitle", "COUPONS READY"))
									]
									+ SVerticalBox::Slot()
									.AutoHeight()
									.HAlign(HAlign_Center)
									.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
									[
										SAssignNew(CouponsReadyText, STextBlock)
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
										.ColorAndOpacity(PrimaryTextColor)
										.Text(FText::FromString(TEXT("0")))
									]
								]
							]
						]
					]
					// Divider
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(Brushes->WhiteBrush.Get())
						.BorderBackgroundColor(DividerColor)
						.Padding(0.0f)
						[
							SNew(SBox)
							.HeightOverride(1.0f)
						]
					]
					// Filter & Sort Toolbar
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(20.0f, 10.0f, 20.0f, 10.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							// Search Box
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
							[
								SNew(SBorder)
								.BorderImage(Brushes->PanelBrush.Get())
								.BorderBackgroundColor(PanelBgColor)
								.Padding(FMargin(8.0f, 4.0f))
								[
									SAssignNew(SearchTextBox, SEditableTextBox)
									.HintText(NSLOCTEXT("SinkMonitor", "SearchHint", "Search sinkable items..."))
									.OnTextChanged(FOnTextChanged::CreateUObject(this, &USinkMonitorInteractWidget::HandleSearchTextChanged))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 14.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "ClearSearchBtn", "Clear"),
									ClearSearchBorder,
									ClearSearchText,
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleClearSearchClicked))
							]
							// Active Only Toggle
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 14.0f, 0.0f))
							[
								SNew(SBorder)
								.BorderImage(Brushes->PanelBrush.Get())
								.BorderBackgroundColor(PanelBgColor)
								.Padding(FMargin(10.0f, 5.0f))
								[
									SAssignNew(ActiveOnlyCheckBox, SCheckBox)
									.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &USinkMonitorInteractWidget::HandleActiveOnlyChanged))
									[
										SNew(STextBlock)
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
										.ColorAndOpacity(PrimaryTextColor)
										.Text(NSLOCTEXT("SinkMonitor", "ActiveOnlyLabel", "Currently Sinking Only (> 0/min)"))
									]
								]
							]
							// Track Filter Buttons
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "TrackAll", "All Tracks"),
									TrackBtnBorders[0],
									TrackBtnTexts[0],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetTrackFilter, ESinkMonitorTrackFilter::All))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "TrackDefault", "Standard"),
									TrackBtnBorders[1],
									TrackBtnTexts[1],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetTrackFilter, ESinkMonitorTrackFilter::DefaultOnly))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "TrackDna", "DNA"),
									TrackBtnBorders[2],
									TrackBtnTexts[2],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetTrackFilter, ESinkMonitorTrackFilter::ExplorationOnly))
							]
						]
						// Sort Row
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
								.ColorAndOpacity(MutedTextColor)
								.Text(NSLOCTEXT("SinkMonitor", "SortByLabel", "SORT BY (PINNED FIRST):"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "SortRate", "Rate / min"),
									SortBtnBorders[0],
									SortBtnTexts[0],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetSortMode, ESinkMonitorSortMode::RateDesc))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "SortPtsMin", "Points / min"),
									SortBtnBorders[1],
									SortBtnTexts[1],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetSortMode, ESinkMonitorSortMode::PointsPerMinDesc))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "SortValue", "Point Value"),
									SortBtnBorders[2],
									SortBtnTexts[2],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetSortMode, ESinkMonitorSortMode::PointValueDesc))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "SortName", "Name (A-Z)"),
									SortBtnBorders[3],
									SortBtnTexts[3],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetSortMode, ESinkMonitorSortMode::NameAsc))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
							[
								MakeToolbarButton(
									NSLOCTEXT("SinkMonitor", "SortTotal", "Lifetime Total"),
									SortBtnBorders[4],
									SortBtnTexts[4],
									FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleSetSortMode, ESinkMonitorSortMode::TotalSunkDesc))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								SAssignNew(ItemCountStatusText, STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								.ColorAndOpacity(MutedTextColor)
								.Text(FText::FromString(TEXT("Showing 0 / 0 items")))
							]
						]
					]
					// Main Split Body
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(FMargin(20.0f, 0.0f, 20.0f, 18.0f))
					[
						SNew(SHorizontalBox)
						// Left Pane: Scrollable Item List
						+ SHorizontalBox::Slot()
						.FillWidth(0.61f)
						.Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(Brushes->PanelBrush.Get())
							.BorderBackgroundColor(PanelBgColor)
							.Padding(FMargin(8.0f, 8.0f))
							[
								SNew(SVerticalBox)
								// Column Headers
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(8.0f, 2.0f, 14.0f, 6.0f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									[
										SNew(STextBlock)
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
										.ColorAndOpacity(MutedTextColor)
										.Text(NSLOCTEXT("SinkMonitor", "ColItem", "ITEM"))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(FMargin(8.0f, 0.0f))
									[
										SNew(SBox)
										.WidthOverride(76.0f)
										.HAlign(HAlign_Right)
										[
											SNew(STextBlock)
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
											.ColorAndOpacity(MutedTextColor)
											.Text(NSLOCTEXT("SinkMonitor", "ColValue", "PTS / EA"))
										]
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(FMargin(8.0f, 0.0f))
									[
										SNew(SBox)
										.WidthOverride(100.0f)
										.HAlign(HAlign_Right)
										[
											SNew(STextBlock)
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
											.ColorAndOpacity(MutedTextColor)
											.Text(NSLOCTEXT("SinkMonitor", "ColRate", "ITEMS / MIN"))
										]
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SBox)
										.WidthOverride(122.0f)
										.HAlign(HAlign_Right)
										[
											SNew(STextBlock)
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
											.ColorAndOpacity(MutedTextColor)
											.Text(NSLOCTEXT("SinkMonitor", "ColPtsMin", "PTS / MIN"))
										]
									]
								]
								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								[
									SAssignNew(ItemListScrollBox, SScrollBox)
								]
							]
						]
						// Right Pane: Detail Inspector
						+ SHorizontalBox::Slot()
						.FillWidth(0.39f)
						[
							SNew(SBorder)
							.BorderImage(Brushes->PanelBrush.Get())
							.BorderBackgroundColor(PanelBgColor)
							.Padding(FMargin(16.0f, 14.0f))
							[
								SNew(SVerticalBox)
								// Selected Item Header
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(FMargin(0.0f, 0.0f, 14.0f, 0.0f))
									[
										SNew(SBorder)
										.BorderImage(Brushes->RowBrush.Get())
										.BorderBackgroundColor(RowBgEvenColor)
										.Padding(FMargin(6.0f))
										[
											SNew(SBox)
											.WidthOverride(68.0f)
											.HeightOverride(68.0f)
											[
												SAssignNew(DetailIconImage, SImage)
												.Image(DetailIconBrush.Get())
											]
										]
									]
									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.VAlign(VAlign_Center)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.AutoHeight()
										[
											SAssignNew(DetailItemNameText, STextBlock)
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
											.ColorAndOpacity(PrimaryTextColor)
											.AutoWrapText(true)
											.Text(NSLOCTEXT("SinkMonitor", "SelectItemPrompt", "Select a sinkable item"))
										]
										+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
											[
												SAssignNew(DetailTrackBadgeBorder, SBorder)
												.BorderImage(Brushes->PillBrush.Get())
												.BorderBackgroundColor(MakeTintBg(FicsitOrangeColor))
												.Padding(FMargin(8.0f, 3.0f))
												[
													SAssignNew(DetailTrackBadgeText, STextBlock)
													.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
													.ColorAndOpacity(FicsitOrangeColor)
													.Text(FText::FromString(TEXT("STANDARD TRACK")))
												]
											]
											+ SHorizontalBox::Slot()
											.AutoWidth()
											[
												MakeToolbarButton(
													NSLOCTEXT("SinkMonitor", "PinToggleBtn", "Pin Item"),
													PinButtonBorder,
													PinButtonText,
													FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandlePinSelectedClicked),
													InactiveBtnColor)
											]
										]
									]
								]
								// Metrics Grid
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.0f, 14.0f, 0.0f, 4.0f))
								[
									MakeDetailMetricRow(NSLOCTEXT("SinkMonitor", "MetricPointVal", "Point Value"), DetailPointValueText, PrimaryTextColor)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
								[
									MakeDetailMetricRow(NSLOCTEXT("SinkMonitor", "MetricRateMin", "Live Sinking Rate"), DetailRatePerMinText, ActiveMintColor)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
								[
									MakeDetailMetricRow(NSLOCTEXT("SinkMonitor", "MetricPtsMin", "Live Points Contributed"), DetailPointsPerMinText, FicsitOrangeColor)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
								[
									MakeDetailMetricRow(NSLOCTEXT("SinkMonitor", "MetricSessionTotal", "Session Total Sunk"), DetailSessionTotalText, PrimaryTextColor)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
								[
									MakeDetailMetricRow(NSLOCTEXT("SinkMonitor", "MetricLifetimeTotal", "Lifetime Total Sunk"), DetailLifetimeTotalText, CyanAccentColor)
								]
								// Active Sinks Header
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(2.0f, 0.0f, 2.0f, 6.0f))
								[
									SAssignNew(DetailActiveSinksHeaderText, STextBlock)
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
									.ColorAndOpacity(FicsitOrangeColor)
									.Text(FText::FromString(TEXT("ACTIVE AWESOME SINKS (0)")))
								]
								// Active Sinks Scroll Box
								+ SVerticalBox::Slot()
								.FillHeight(1.0f)
								[
									SNew(SBorder)
									.BorderImage(Brushes->RowBrush.Get())
									.BorderBackgroundColor(RowBgEvenColor)
									.Padding(FMargin(8.0f, 6.0f))
									[
										SAssignNew(ActiveSinksScrollBox, SScrollBox)
									]
								]
							]
						]
					]
				]
			]
		];
#endif // UE_SERVER
}

void USinkMonitorInteractWidget::NativeConstruct()
{
	Super::NativeConstruct();

#if !UE_SERVER
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (ASinkMonitorSubsystem* Sub = ResolveSubsystem())
	{
		Sub->OnSinkStatsUpdated.AddUniqueDynamic(this, &USinkMonitorInteractWidget::HandleSubsystemStatsUpdated);
		Sub->RequestStatsSync();
	}

	UpdateFilterAndSortVisuals();
	RefreshData();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&USinkMonitorInteractWidget::RefreshData,
			0.5f,
			true);
	}
#endif
}

void USinkMonitorInteractWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

#if !UE_SERVER
	if (ASinkMonitorSubsystem* Sub = BoundSubsystem.Get())
	{
		Sub->OnSinkStatsUpdated.RemoveDynamic(this, &USinkMonitorInteractWidget::HandleSubsystemStatsUpdated);
	}
#endif
	BoundSubsystem.Reset();

	Super::NativeDestruct();
}

FReply USinkMonitorInteractWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnEscapePressed();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#if !UE_SERVER
ASinkMonitorSubsystem* USinkMonitorInteractWidget::ResolveSubsystem()
{
	if (ASinkMonitorSubsystem* Existing = BoundSubsystem.Get())
	{
		return Existing;
	}
	if (UWorld* World = GetWorld())
	{
		ASinkMonitorSubsystem* Sub = ASinkMonitorSubsystem::Get(World);
		BoundSubsystem = Sub;
		return Sub;
	}
	return nullptr;
}

void USinkMonitorInteractWidget::HandleSubsystemStatsUpdated()
{
	RefreshData();
}

void USinkMonitorInteractWidget::HandleSearchTextChanged(const FText& NewText)
{
	CurrentSearchQuery = NewText.ToString().TrimStartAndEnd();
	RefreshData();
}

void USinkMonitorInteractWidget::HandleActiveOnlyChanged(ECheckBoxState NewState)
{
	bShowActiveOnly = (NewState == ECheckBoxState::Checked);
	RefreshData();
}

FReply USinkMonitorInteractWidget::HandleClearSearchClicked()
{
	CurrentSearchQuery.Empty();
	if (SearchTextBox.IsValid())
	{
		SearchTextBox->SetText(FText::GetEmpty());
	}
	RefreshData();
	return FReply::Handled();
}

FReply USinkMonitorInteractWidget::HandleSetTrackFilter(ESinkMonitorTrackFilter NewFilter)
{
	TrackFilter = NewFilter;
	UpdateFilterAndSortVisuals();
	RefreshData();
	return FReply::Handled();
}

FReply USinkMonitorInteractWidget::HandleSetSortMode(ESinkMonitorSortMode NewMode)
{
	SortMode = NewMode;
	UpdateFilterAndSortVisuals();
	RefreshData();
	return FReply::Handled();
}

FReply USinkMonitorInteractWidget::HandleRowClicked(TSubclassOf<UFGItemDescriptor> ClickedItemClass)
{
	SelectedItemClass = ClickedItemClass;
	RefreshData();
	return FReply::Handled();
}

FReply USinkMonitorInteractWidget::HandlePinSelectedClicked()
{
	if (SelectedItemClass)
	{
		if (ASinkMonitorSubsystem* Sub = ResolveSubsystem())
		{
			Sub->TogglePinItem(SelectedItemClass);
			RefreshData();
		}
	}
	return FReply::Handled();
}

FReply USinkMonitorInteractWidget::HandleResetSessionClicked()
{
	if (ASinkMonitorSubsystem* Sub = ResolveSubsystem())
	{
		Sub->RequestResetSessionStats();
		RefreshData();
	}
	return FReply::Handled();
}

FReply USinkMonitorInteractWidget::HandleCloseClicked()
{
	OnEscapePressed();
	return FReply::Handled();
}

void USinkMonitorInteractWidget::UpdateFilterAndSortVisuals()
{
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bActive = static_cast<int32>(TrackFilter) == i;
		if (TrackBtnBorders[i].IsValid())
		{
			TrackBtnBorders[i]->SetBorderBackgroundColor(bActive ? ActiveBtnColor : InactiveBtnColor);
		}
	}
	for (int32 i = 0; i < 5; ++i)
	{
		const bool bActive = static_cast<int32>(SortMode) == i;
		if (SortBtnBorders[i].IsValid())
		{
			SortBtnBorders[i]->SetBorderBackgroundColor(bActive ? ActiveBtnColor : InactiveBtnColor);
		}
	}
}

void USinkMonitorInteractWidget::RefreshData()
{
	ASinkMonitorSubsystem* Sub = ResolveSubsystem();
	if (!Sub)
	{
		return;
	}

	const TArray<FSinkableItemEntry> AllItems = Sub->GetAllSinkableItems();

	float TotalPtsPerMin = 0.0f;
	int32 ActiveItemCount = 0;
	int32 TotalActiveSinks = 0;
	Sub->GetGlobalSummary(TotalPtsPerMin, ActiveItemCount, TotalActiveSinks);

	float StandardPtsPerMin = 0.0f;
	float DnaPtsPerMin = 0.0f;
	for (const FSinkableItemEntry& Entry : AllItems)
	{
		if (Entry.CurrentItemsPerMin > 0.0f)
		{
			if (Entry.SinkTrack == EResourceSinkTrack::RST_Exploration)
			{
				DnaPtsPerMin += Entry.CurrentPointsPerMin;
			}
			else
			{
				StandardPtsPerMin += Entry.CurrentPointsPerMin;
			}
		}
	}

	AFGResourceSinkSubsystem* SinkSub = GetWorld() ? AFGResourceSinkSubsystem::Get(GetWorld()) : nullptr;
	UpdateCouponTrackWidgets(
		SinkSub,
		EResourceSinkTrack::RST_Default,
		StandardPtsPerMin,
		StandardCouponPtsText,
		StandardCouponMetaText,
		StandardCouponProgressBar);
	UpdateCouponTrackWidgets(
		SinkSub,
		EResourceSinkTrack::RST_Exploration,
		DnaPtsPerMin,
		DnaCouponPtsText,
		DnaCouponMetaText,
		DnaCouponProgressBar);
	if (CouponsReadyText.IsValid())
	{
		CouponsReadyText->SetText(SinkSub
			? FText::FromString(FormatIntWithCommas(SinkSub->GetNumCoupons()))
			: FText::FromString(TEXT("0")));
	}

	if (HeaderSummaryText.IsValid())
	{
		HeaderSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  •  %d Active %s  •  %d Active %s"),
			*FormatPointsPerMin(TotalPtsPerMin),
			ActiveItemCount,
			ActiveItemCount == 1 ? TEXT("Item") : TEXT("Items"),
			TotalActiveSinks,
			TotalActiveSinks == 1 ? TEXT("Sink") : TEXT("Sinks"))));
	}

	TArray<FSinkableItemEntry> Filtered;
	Filtered.Reserve(AllItems.Num());
	for (const FSinkableItemEntry& Entry : AllItems)
	{
		if (bShowActiveOnly && Entry.CurrentItemsPerMin <= 0.0f)
		{
			continue;
		}
		if (TrackFilter == ESinkMonitorTrackFilter::DefaultOnly && Entry.SinkTrack != EResourceSinkTrack::RST_Default)
		{
			continue;
		}
		if (TrackFilter == ESinkMonitorTrackFilter::ExplorationOnly && Entry.SinkTrack != EResourceSinkTrack::RST_Exploration)
		{
			continue;
		}
		if (!CurrentSearchQuery.IsEmpty() && !Entry.DisplayName.ToString().Contains(CurrentSearchQuery, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Filtered.Add(Entry);
	}

	Filtered.Sort([this](const FSinkableItemEntry& A, const FSinkableItemEntry& B)
	{
		if (A.bIsPinned != B.bIsPinned)
		{
			return A.bIsPinned;
		}

		switch (SortMode)
		{
		case ESinkMonitorSortMode::RateDesc:
			if (!FMath::IsNearlyEqual(A.CurrentItemsPerMin, B.CurrentItemsPerMin))
			{
				return A.CurrentItemsPerMin > B.CurrentItemsPerMin;
			}
			break;
		case ESinkMonitorSortMode::PointsPerMinDesc:
			if (!FMath::IsNearlyEqual(A.CurrentPointsPerMin, B.CurrentPointsPerMin))
			{
				return A.CurrentPointsPerMin > B.CurrentPointsPerMin;
			}
			break;
		case ESinkMonitorSortMode::PointValueDesc:
			if (A.PointValue != B.PointValue)
			{
				return A.PointValue > B.PointValue;
			}
			break;
		case ESinkMonitorSortMode::TotalSunkDesc:
			if (A.LifetimeTotalSunk != B.LifetimeTotalSunk)
			{
				return A.LifetimeTotalSunk > B.LifetimeTotalSunk;
			}
			break;
		case ESinkMonitorSortMode::NameAsc:
		default:
			break;
		}
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});

	if (ItemCountStatusText.IsValid())
	{
		ItemCountStatusText->SetText(FText::FromString(FString::Printf(
			TEXT("Showing %d / %d sinkable items"),
			Filtered.Num(),
			AllItems.Num())));
	}

	// Auto-select first item if current selection is missing or unset.
	const bool bSelectionPresent = SelectedItemClass != nullptr && AllItems.ContainsByPredicate(
		[this](const FSinkableItemEntry& Entry) { return Entry.ItemClass == SelectedItemClass; });
	if (!bSelectionPresent && !Filtered.IsEmpty())
	{
		SelectedItemClass = Filtered[0].ItemClass;
	}

	RebuildOrUpdateRows(Filtered);
	UpdateDetailPanel(AllItems);
}

bool USinkMonitorInteractWidget::DoesRowOrderMatch(const TArray<FSinkableItemEntry>& FilteredItems) const
{
	if (RowWidgets.Num() != FilteredItems.Num())
	{
		return false;
	}
	for (int32 i = 0; i < FilteredItems.Num(); ++i)
	{
		if (RowWidgets[i].ItemClass != FilteredItems[i].ItemClass)
		{
			return false;
		}
	}
	return true;
}

void USinkMonitorInteractWidget::UpdateRowVisuals(
	FItemRowWidgets& Row,
	const FSinkableItemEntry& Entry,
	bool bSelected)
{
	const bool bActive = Entry.CurrentItemsPerMin > 0.0f;
	const FLinearColor BaseRowColor = (Row.RowIndex % 2 == 0) ? RowBgEvenColor : RowBgOddColor;

	if (Row.RowBorder.IsValid())
	{
		Row.RowBorder->SetBorderBackgroundColor(bSelected ? RowSelectedBgColor : BaseRowColor);
	}
	if (Row.PinText.IsValid())
	{
		Row.PinText->SetText(FText::FromString(Entry.bIsPinned ? TEXT("*") : TEXT("")));
		Row.PinText->SetColorAndOpacity(Entry.bIsPinned ? FicsitOrangeColor : MutedTextColor);
	}
	if (Row.NameText.IsValid())
	{
		Row.NameText->SetText(Entry.DisplayName);
		Row.NameText->SetColorAndOpacity(bActive || bSelected ? PrimaryTextColor : FLinearColor(0.82f, 0.85f, 0.90f, 1.0f));
	}
	if (Row.TrackTagBorder.IsValid())
	{
		const bool bDna = Entry.SinkTrack == EResourceSinkTrack::RST_Exploration;
		Row.TrackTagBorder->SetVisibility(bDna ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (Row.PointValueText.IsValid())
	{
		Row.PointValueText->SetText(FText::FromString(FormatIntWithCommas(Entry.PointValue)));
	}
	if (Row.RateText.IsValid())
	{
		Row.RateText->SetText(FText::FromString(FormatRatePerMin(Entry.CurrentItemsPerMin)));
		Row.RateText->SetColorAndOpacity(bActive ? ActiveMintColor : MutedTextColor);
	}
	if (Row.PointsPerMinText.IsValid())
	{
		Row.PointsPerMinText->SetText(FText::FromString(
			bActive ? FormatIntWithCommas(FMath::RoundToInt64(Entry.CurrentPointsPerMin)) : TEXT("0")));
		Row.PointsPerMinText->SetColorAndOpacity(bActive ? FicsitOrangeColor : MutedTextColor);
	}
}

void USinkMonitorInteractWidget::RebuildOrUpdateRows(const TArray<FSinkableItemEntry>& FilteredItems)
{
	if (!ItemListScrollBox.IsValid() || !Brushes.IsValid())
	{
		return;
	}

	if (DoesRowOrderMatch(FilteredItems))
	{
		for (int32 i = 0; i < FilteredItems.Num(); ++i)
		{
			UpdateRowVisuals(RowWidgets[i], FilteredItems[i], FilteredItems[i].ItemClass == SelectedItemClass);
		}
		return;
	}

	ItemListScrollBox->ClearChildren();
	RowWidgets.Reset();
	RowWidgets.Reserve(FilteredItems.Num());

	for (int32 i = 0; i < FilteredItems.Num(); ++i)
	{
		const FSinkableItemEntry& Entry = FilteredItems[i];

		FItemRowWidgets Row;
		Row.ItemClass = Entry.ItemClass;
		Row.RowIndex = i;
		Row.IconBrush = MakeShared<FSlateBrush>();
		Row.IconBrush->DrawAs = ESlateBrushDrawType::NoDrawType;

		TSharedRef<SWidget> RowWidget = SNew(SButton)
			.ButtonStyle(&Brushes->TransparentButtonStyle)
			.OnClicked(FOnClicked::CreateUObject(this, &USinkMonitorInteractWidget::HandleRowClicked, Entry.ItemClass))
			[
				SAssignNew(Row.RowBorder, SBorder)
				.BorderImage(Brushes->RowBrush.Get())
				.BorderBackgroundColor(RowBgEvenColor)
				.Padding(FMargin(8.0f, 6.0f))
				[
					SNew(SHorizontalBox)
					// Pin badge
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(16.0f)
						.HAlign(HAlign_Center)
						[
							SAssignNew(Row.PinText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
							.ColorAndOpacity(FicsitOrangeColor)
						]
					]
					// Icon
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(SBox)
						.WidthOverride(28.0f)
						.HeightOverride(28.0f)
						[
							SAssignNew(Row.IconImage, SImage)
							.Image(Row.IconBrush.Get())
						]
					]
					// Item Name + Optional DNA tag
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SAssignNew(Row.NameText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(PrimaryTextColor)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
						[
							SAssignNew(Row.TrackTagBorder, SBorder)
							.BorderImage(Brushes->PillBrush.Get())
							.BorderBackgroundColor(MakeTintBg(CyanAccentColor))
							.Padding(FMargin(5.0f, 1.0f))
							[
								SAssignNew(Row.TrackTagText, STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
								.ColorAndOpacity(CyanAccentColor)
								.Text(FText::FromString(TEXT("DNA")))
							]
						]
					]
					// Point Value
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.0f, 0.0f))
					[
						SNew(SBox)
						.WidthOverride(76.0f)
						.HAlign(HAlign_Right)
						[
							SAssignNew(Row.PointValueText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(MutedTextColor)
						]
					]
					// Items / min
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.0f, 0.0f))
					[
						SNew(SBox)
						.WidthOverride(100.0f)
						.HAlign(HAlign_Right)
						[
							SAssignNew(Row.RateText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]
					]
					// Points / min
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(116.0f)
						.HAlign(HAlign_Right)
						[
							SAssignNew(Row.PointsPerMinText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
					]
				]
			];

		UTexture2D* SmallIcon = Entry.ItemClass ? UFGItemDescriptor::GetSmallIcon(Entry.ItemClass) : nullptr;
		SyncBrushToTexture(Row.IconBrush, Row.IconImage, SmallIcon, 28.0f, CachedIconTextures);
		UpdateRowVisuals(Row, Entry, Entry.ItemClass == SelectedItemClass);

		ItemListScrollBox->AddSlot()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 3.0f))
			[
				RowWidget
			];

		RowWidgets.Add(MoveTemp(Row));
	}
}

void USinkMonitorInteractWidget::UpdateDetailPanel(const TArray<FSinkableItemEntry>& AllItems)
{
	const FSinkableItemEntry* SelectedEntry = AllItems.FindByPredicate(
		[this](const FSinkableItemEntry& Entry) { return Entry.ItemClass == SelectedItemClass; });

	if (!SelectedEntry)
	{
		if (DetailItemNameText.IsValid())
		{
			DetailItemNameText->SetText(NSLOCTEXT("SinkMonitor", "NoItemSelected", "Select a sinkable item"));
		}
		if (ActiveSinksScrollBox.IsValid())
		{
			ActiveSinksScrollBox->ClearChildren();
		}
		return;
	}

	UTexture2D* IconTex = UFGItemDescriptor::GetBigIcon(SelectedEntry->ItemClass);
	if (!IconTex)
	{
		IconTex = UFGItemDescriptor::GetSmallIcon(SelectedEntry->ItemClass);
	}
	SyncBrushToTexture(DetailIconBrush, DetailIconImage, IconTex, 68.0f, CachedIconTextures);

	if (DetailItemNameText.IsValid())
	{
		DetailItemNameText->SetText(SelectedEntry->DisplayName);
	}

	const bool bDna = SelectedEntry->SinkTrack == EResourceSinkTrack::RST_Exploration;
	const FLinearColor TrackAccent = bDna ? CyanAccentColor : FicsitOrangeColor;
	if (DetailTrackBadgeBorder.IsValid())
	{
		DetailTrackBadgeBorder->SetBorderBackgroundColor(MakeTintBg(TrackAccent));
	}
	if (DetailTrackBadgeText.IsValid())
	{
		DetailTrackBadgeText->SetColorAndOpacity(TrackAccent);
		DetailTrackBadgeText->SetText(FText::FromString(bDna ? TEXT("EXPLORATION / DNA") : TEXT("STANDARD TRACK")));
	}

	if (PinButtonBorder.IsValid())
	{
		PinButtonBorder->SetBorderBackgroundColor(SelectedEntry->bIsPinned ? ActiveBtnColor : InactiveBtnColor);
	}
	if (PinButtonText.IsValid())
	{
		PinButtonText->SetText(FText::FromString(SelectedEntry->bIsPinned ? TEXT("Pinned to Top") : TEXT("Pin to Top")));
	}

	if (DetailPointValueText.IsValid())
	{
		DetailPointValueText->SetText(FText::FromString(
			FString::Printf(TEXT("%s pts"), *FormatIntWithCommas(SelectedEntry->PointValue))));
	}
	if (DetailRatePerMinText.IsValid())
	{
		DetailRatePerMinText->SetText(FText::FromString(FormatRatePerMin(SelectedEntry->CurrentItemsPerMin)));
		DetailRatePerMinText->SetColorAndOpacity(
			SelectedEntry->CurrentItemsPerMin > 0.0f ? ActiveMintColor : MutedTextColor);
	}
	if (DetailPointsPerMinText.IsValid())
	{
		DetailPointsPerMinText->SetText(FText::FromString(FormatPointsPerMin(SelectedEntry->CurrentPointsPerMin)));
		DetailPointsPerMinText->SetColorAndOpacity(
			SelectedEntry->CurrentItemsPerMin > 0.0f ? FicsitOrangeColor : MutedTextColor);
	}
	if (DetailSessionTotalText.IsValid())
	{
		DetailSessionTotalText->SetText(FText::FromString(
			FString::Printf(TEXT("%s items"), *FormatIntWithCommas(SelectedEntry->SessionTotalSunk))));
	}
	if (DetailLifetimeTotalText.IsValid())
	{
		DetailLifetimeTotalText->SetText(FText::FromString(
			FString::Printf(TEXT("%s items"), *FormatIntWithCommas(SelectedEntry->LifetimeTotalSunk))));
	}

	if (!ActiveSinksScrollBox.IsValid() || !Brushes.IsValid())
	{
		return;
	}

	TArray<FSinkActiveSourceInfo> Sources;
	if (ASinkMonitorSubsystem* Sub = ResolveSubsystem())
	{
		Sources = Sub->GetActiveSinksForItem(SelectedEntry->ItemClass);
	}

	if (DetailActiveSinksHeaderText.IsValid())
	{
		DetailActiveSinksHeaderText->SetText(FText::FromString(
			FString::Printf(TEXT("ACTIVE AWESOME SINKS (%d)"), Sources.Num())));
	}

	ActiveSinksScrollBox->ClearChildren();
	if (Sources.IsEmpty())
	{
		ActiveSinksScrollBox->AddSlot()
			.Padding(FMargin(4.0f, 6.0f))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(MutedTextColor)
				.AutoWrapText(true)
				.Text(NSLOCTEXT("SinkMonitor", "NoActiveSinks", "No AWESOME Sinks are currently consuming this item."))
			];
		return;
	}

	const AActor* TerminalActor = Cast<AActor>(mInteractObject.Get());
	const bool bHasTerminalLoc = TerminalActor != nullptr;
	const FVector TerminalLoc = bHasTerminalLoc ? TerminalActor->GetActorLocation() : FVector::ZeroVector;

	for (int32 i = 0; i < Sources.Num(); ++i)
	{
		const FSinkActiveSourceInfo& Info = Sources[i];
		const float DistMeters = bHasTerminalLoc
			? FVector::Dist(TerminalLoc, Info.WorldLocation) * 0.01f
			: 0.0f;

		const FString CoordLine = bHasTerminalLoc
			? FString::Printf(
				TEXT("%.0f m away  (X: %.0fm, Y: %.0fm, Z: %.0fm)"),
				DistMeters,
				Info.WorldLocation.X * 0.01f,
				Info.WorldLocation.Y * 0.01f,
				Info.WorldLocation.Z * 0.01f)
			: FString::Printf(
				TEXT("X: %.0fm, Y: %.0fm, Z: %.0fm"),
				Info.WorldLocation.X * 0.01f,
				Info.WorldLocation.Y * 0.01f,
				Info.WorldLocation.Z * 0.01f);

		ActiveSinksScrollBox->AddSlot()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
			[
				SNew(SBorder)
				.BorderImage(Brushes->PanelBrush.Get())
				.BorderBackgroundColor(PanelBgColor)
				.Padding(FMargin(10.0f, 6.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(PrimaryTextColor)
							.Text(FText::FromString(FString::Printf(TEXT("AWESOME Sink #%d"), i + 1)))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(ActiveMintColor)
							.Text(FText::FromString(FormatRatePerMin(Info.ItemsPerMin)))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(MutedTextColor)
						.Text(FText::FromString(CoordLine))
					]
				]
			];
	}
}
#else
void USinkMonitorInteractWidget::HandleSubsystemStatsUpdated()
{
}
#endif // !UE_SERVER
