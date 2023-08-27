// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "MessageLogModule.h"
#include "Misc/UObjectToken.h"
#include "Logging/MessageLog.h"
#include "Internationalization/Regex.h"
#include "Widgets/Notifications/INotificationWidget.h"

class SVoxelNotification : public SCompoundWidget, public INotificationWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ATTRIBUTE(int32, Count);
	};

	void Construct(
		const FArguments& Args,
		const TSharedRef<FTokenizedMessage>& Message)
	{
		const TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);

		TSharedPtr<SHorizontalBox> HBox;
		VBox->AddSlot()
		.AutoHeight()
		[
			SAssignNew(HBox, SHorizontalBox)
		];

		for (const TSharedRef<IMessageToken>& Token : Message->GetMessageTokens())
		{
			CreateMessage(VBox, HBox, Token, 2.0f);
		}

		HBox->AddSlot();

		HBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility_Lambda([=]
			{
				return Args._Count.Get() == 1 ? EVisibility::Collapsed : EVisibility::Visible;
			})
			.TextStyle(FAppStyle::Get(), "MessageLog")
			.Text_Lambda([=]
			{
				return FText::Format(INVTEXT(" (x{0})"), FText::AsNumber(Args._Count.Get()));
			})
		];

		HBox->AddSlot()
		.AutoWidth()
		[
			SNullWidget::NullWidget
		];

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.Padding(2.0f)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(16)
					.HeightOverride(16)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush(FTokenizedMessage::GetSeverityIconName(Message->GetSeverity())))
					]
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MaxDesiredWidth(1200)
				[
					VBox
				]
			]
		];
	}

	//~ Begin INotificationWidget Interface
	virtual void OnSetCompletionState(SNotificationItem::ECompletionState State) override
	{

	}
	virtual TSharedRef<SWidget> AsWidget() override
	{
		return AsShared();
	}
	//~ End INotificationWidget Interface

private:
	static TSharedRef<SWidget> CreateHyperlink(const TSharedRef<IMessageToken>& MessageToken, const FText& Tooltip)
	{
		return SNew(SHyperlink)
			.Text(MessageToken->ToText())
			.ToolTipText(Tooltip)
			.TextStyle(FAppStyle::Get(), "MessageLog")
			.OnNavigate_Lambda([=]
			{
				MessageToken->GetOnMessageTokenActivated().ExecuteIfBound(MessageToken);
			});
	}

	static void CreateMessage(
		const TSharedRef<SVerticalBox>& VBox,
		TSharedPtr<SHorizontalBox>& HBox,
		const TSharedRef<IMessageToken>& MessageToken,
		const float Padding)
	{
		const auto AddWidget = [&](
			const TSharedRef<SWidget>& Widget,
			const FName IconName = {},
			const TAttribute<EVisibility>& WidgetVisibility = {})
		{
			const TSharedRef<SHorizontalBox> ChildHBox = SNew(SHorizontalBox);

			if (WidgetVisibility.IsBound())
			{
				ChildHBox->SetVisibility(WidgetVisibility);
			}

			if (!IconName.IsNone())
			{
				ChildHBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush(IconName))
				];
			}

			ChildHBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				Widget
			];

			HBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(Padding, 0.0f, 0.0f, 0.0f)
			[
				ChildHBox
			];
		};

		switch (MessageToken->GetType())
		{
		default:
		{
			ensure(false);
			return;
		}

		case EMessageToken::Severity:
		{
			return;
		}

		case EMessageToken::Image:
		{
			const TSharedRef<FImageToken> ImageToken = StaticCastSharedRef<FImageToken>(MessageToken);

			if (ImageToken->GetImageName().IsNone())
			{
				return;
			}

			if (MessageToken->GetOnMessageTokenActivated().IsBound())
			{
				AddWidget(
					SNew(SButton)
					.OnClicked_Lambda([=]
					{
						MessageToken->GetOnMessageTokenActivated().ExecuteIfBound(MessageToken);
						return FReply::Handled();
					})
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush(ImageToken->GetImageName()))
					]);
			}
			else
			{
				AddWidget(
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush(ImageToken->GetImageName()))
				);
			}
		}
		break;

		case EMessageToken::Object:
		{
			const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(MessageToken);

			UObject* Object;

			// Due to blueprint reconstruction, we can't directly use the Object as it will get trashed during the blueprint reconstruction and the message token will no longer point to the right UObject.
			// Instead we will retrieve the object from the name which should always be good.
			if (UObjectToken->GetObject().IsValid())
			{
				if (!UObjectToken->ToText().ToString().Equals(UObjectToken->GetObject().Get()->GetName()))
				{
					Object = FindObject<UObject>(nullptr, *UObjectToken->GetOriginalObjectPathName());
				}
				else
				{
					Object = UObjectToken->GetObject().Get();
				}
			}
			else
			{
				// We have no object (probably because is now stale), try finding the original object linked to this message token to see if it still exist
				Object = FindObject<UObject>(nullptr, *UObjectToken->GetOriginalObjectPathName());
			}

			AddWidget(
				CreateHyperlink(MessageToken, FUObjectToken::DefaultOnGetObjectDisplayName().IsBound() ? FUObjectToken::DefaultOnGetObjectDisplayName().Execute(Object, true) : UObjectToken->ToText()),
				"Icons.Search");
		}
		break;

		case EMessageToken::URL:
		{
			const TSharedRef<FURLToken> URLToken = StaticCastSharedRef<FURLToken>(MessageToken);

			AddWidget(
				CreateHyperlink(MessageToken, FText::FromString(URLToken->GetURL())),
				"MessageLog.Url");
		}
		break;

		case EMessageToken::EdGraph:
		{
			AddWidget(
				CreateHyperlink(MessageToken, MessageToken->ToText()),
				"Icons.Search");
		}
		break;

		case EMessageToken::Action:
		{
			const TSharedRef<FActionToken> ActionToken = StaticCastSharedRef<FActionToken>(MessageToken);

			const TSharedRef<SHyperlink> Widget =
				SNew(SHyperlink)
				.Text(MessageToken->ToText())
				.ToolTipText(ActionToken->GetActionDescription())
				.TextStyle(FAppStyle::Get(), "MessageLog")
				.OnNavigate_Lambda([=]
				{
					ActionToken->ExecuteAction();
				});

			AddWidget(
				Widget,
				"MessageLog.Action",
				MakeAttributeLambda([=]
				{
					return ActionToken->CanExecuteAction() ? EVisibility::Visible : EVisibility::Collapsed;
				}));
		}
		break;

		case EMessageToken::AssetName:
		{
			const TSharedRef<FAssetNameToken> AssetNameToken = StaticCastSharedRef<FAssetNameToken>(MessageToken);

			AddWidget(
				CreateHyperlink(MessageToken, AssetNameToken->ToText()),
				"Icons.Search");
		}
		break;

		case EMessageToken::DynamicText:
		{
			const TSharedRef<FDynamicTextToken> TextToken = StaticCastSharedRef<FDynamicTextToken>(MessageToken);

			AddWidget(
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(TextToken->GetTextAttribute()));
		}
		break;

		case EMessageToken::Text:
		{
			if (MessageToken->GetOnMessageTokenActivated().IsBound())
			{
				AddWidget(CreateHyperlink(MessageToken, MessageToken->ToText()));
				return;
			}

			FString MessageString = MessageToken->ToText().ToString();
			if (MessageString == "\n")
			{
				VBox->AddSlot()
				.AutoHeight()
				[
					SAssignNew(HBox, SHorizontalBox)
				];
				return;
			}

			// ^((?:[\w]\:|\\)(?:(?:\\[a-z_\-\s0-9\.]+)+)\.(?:cpp|h))\((\d+)\)
			// https://regex101.com/r/vV4cV7/1
			const FRegexPattern FileAndLinePattern(TEXT("^((?:[\\w]\\:|\\\\)(?:(?:\\\\[a-z_\\-\\s0-9\\.]+)+)\\.(?:cpp|h))\\((\\d+)\\)"));
			FRegexMatcher FileAndLineRegexMatcher(FileAndLinePattern, MessageString);

			TSharedRef<SWidget> SourceLink = SNullWidget::NullWidget;

			if (FileAndLineRegexMatcher.FindNext())
			{
				const FString FileName = FileAndLineRegexMatcher.GetCaptureGroup(1);
				const int32 LineNumber = FCString::Atoi(*FileAndLineRegexMatcher.GetCaptureGroup(2));

				// Remove the hyperlink from the message, since we're splitting it into its own string.
				MessageString.RightChopInline(FileAndLineRegexMatcher.GetMatchEnding(), false);

				SourceLink = SNew(SHyperlink)
					.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
					.TextStyle(FAppStyle::Get(), "MessageLog")
					.OnNavigate_Lambda([=] { FSlateApplication::Get().GotoLineInSource(FileName, LineNumber); })
					.Text(FText::FromString(FileAndLineRegexMatcher.GetCaptureGroup(0)));
			}

			const TSharedRef<SHorizontalBox> Widget =
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0)
				[
					SourceLink
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(0.f, 4.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(MessageString))
					.ColorAndOpacity(FSlateColor::UseForeground())
					.TextStyle(FAppStyle::Get(), "MessageLog")
				];

			AddWidget(Widget);
		}
		break;

		case EMessageToken::Actor:
		{
			const TSharedRef<FActorToken> ActorToken = StaticCastSharedRef<FActorToken>(MessageToken);

			AddWidget(
				CreateHyperlink(MessageToken, ActorToken->ToText()),
				"Icons.Search");
		}
		break;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelMessagesEditor
{
public:
	void LogMessage(const TSharedRef<FTokenizedMessage>& Message)
	{
		if (GEditor->PlayWorld ||
			GIsPlayInEditorWorld)
		{
			FMessageLog("PIE").AddMessage(Message);
		}

		FMessageLog("Voxel").AddMessage(Message);

		const FText MessageText = Message->ToText();
		const double Time = FPlatformTime::Seconds();

		bool bFoundRecentMessage = false;
		for (int32 Index = 0; Index < RecentMessages.Num(); Index++)
		{
			FRecentMessage& RecentMessage = RecentMessages[Index];
			if (RecentMessage.LastTime + 5 < Time)
			{
				RecentMessages.RemoveAtSwap(Index);
				Index--;
				continue;
			}

			if (RecentMessage.Text.EqualTo(MessageText))
			{
				bFoundRecentMessage = true;
				RecentMessage.LastTime = Time;
				continue;
			}
		}

		if (!bFoundRecentMessage)
		{
			RecentMessages.Add(FRecentMessage
			{
				MessageText,
				Time
			});
		}

		if (RecentMessages.Num() > 3)
		{
			for (const TSharedPtr<FNotification>& Notification : Notifications)
			{
				if (const TSharedPtr<SNotificationItem> Item = Notification->WeakItem.Pin())
				{
					Item->SetExpireDuration(0);
					Item->SetFadeOutDuration(0);
					Item->ExpireAndFadeout();
				}
			}
			Notifications.Reset();

			const FText ErrorText = FText::FromString(FString::FromInt(RecentMessages.Num()) + " voxel errors");

			if (const TSharedPtr<SNotificationItem> Item = WeakGlobalNotification.Pin())
			{
				Item->SetText(ErrorText);
				// Reset expiration
				Item->ExpireAndFadeout();
				return;
			}

			FNotificationInfo Info = FNotificationInfo(ErrorText);
			Info.CheckBoxState = ECheckBoxState::Unchecked;
			Info.ExpireDuration = 10;
			Info.WidthOverride = FOptionalSize();

			Info.ButtonDetails.Add(FNotificationButtonInfo(
				INVTEXT("Dismiss"),
				FText(),
				MakeLambdaDelegate([this]
				{
					if (const TSharedPtr<SNotificationItem> Item = WeakGlobalNotification.Pin())
					{
						Item->SetFadeOutDuration(0);
						Item->SetFadeOutDuration(0);
						Item->ExpireAndFadeout();
					}
				}),
				SNotificationItem::CS_Fail));

			Info.ButtonDetails.Add(FNotificationButtonInfo(
				INVTEXT("Show Message Log"),
				FText(),
				MakeLambdaDelegate([this]
				{
					FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
					MessageLogModule.OpenMessageLog("Voxel");
				}),
				SNotificationItem::CS_Fail));

			const TSharedPtr<SNotificationItem> GlobalNotification = FSlateNotificationManager::Get().AddNotification(Info);
			if (!ensure(GlobalNotification))
			{
				return;
			}

			GlobalNotification->SetCompletionState(SNotificationItem::CS_Fail);;
			WeakGlobalNotification = GlobalNotification;

			return;
		}

		for (int32 Index = 0; Index < Notifications.Num(); Index++)
		{
			const TSharedPtr<FNotification> Notification = Notifications[Index];
			const TSharedPtr<SNotificationItem> Item = Notification->WeakItem.Pin();
			if (!Item.IsValid())
			{
				Notifications.RemoveAtSwap(Index);
				Index--;
				continue;
			}

			if (!Notification->Text.EqualToCaseIgnored(MessageText))
			{
				continue;
			}

			Notification->Count++;
			Item->ExpireAndFadeout();
			return;
		}

		const TSharedRef<FNotification> Notification = MakeVoxelShared<FNotification>();
		Notification->Text = MessageText;

		FNotificationInfo Info = FNotificationInfo(MessageText);
		Info.CheckBoxState = ECheckBoxState::Unchecked;
		Info.ExpireDuration = 10;
		Info.WidthOverride = FOptionalSize();
		Info.ContentWidget =
			SNew(SVoxelNotification, Message)
			.Count_Lambda([=]
			{
				return Notification->Count;
			});

		Notification->WeakItem = FSlateNotificationManager::Get().AddNotification(Info);
		Notifications.Add(Notification);
	}

private:
	TWeakPtr<SNotificationItem> WeakGlobalNotification;

	struct FRecentMessage
	{
		FText Text;
		double LastTime = 0;
	};
	TVoxelArray<FRecentMessage> RecentMessages;

	struct FNotification
	{
		TWeakPtr<SNotificationItem> WeakItem;
		FText Text;
		int32 Count = 1;
	};
	TVoxelArray<TSharedPtr<FNotification>> Notifications;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMessagesEditor* GVoxelMessagesEditor = nullptr;

VOXEL_RUN_ON_STARTUP_EDITOR(RegisterMessageEditor)
{
	GVoxelMessagesEditor = new FVoxelMessagesEditor();

	FVoxelMessages::OnMessageLogged.AddLambda([=](const TSharedRef<FTokenizedMessage>& Message)
	{
		GVoxelMessagesEditor->LogMessage(Message);
	});

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	InitOptions.bShowPages = false;
	InitOptions.bAllowClear = true;
	MessageLogModule.RegisterLogListing("Voxel", INVTEXT("Voxel"), InitOptions);
}