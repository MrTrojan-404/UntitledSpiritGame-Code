// Fill out your copyright notice in the Description page of Project Settings.


#include "ReturnToMainMenu.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/Button.h"
#include "GameAnimationSample/Character/GASPPlayerCharacter.h"
#include "GameAnimationSample/GameMode/GASPCoopGameMode.h"
#include "GameAnimationSample/PlayerController/GASPCorePlayerController.h"
#include "GameFramework/GameModeBase.h"

void UReturnToMainMenu::MenuSetup()
{
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (ReturnButton) ReturnButton->SetIsEnabled(true);
	if (RestartLevelButton) RestartLevelButton->SetIsEnabled(true);
	if (RestartCheckpointButton) RestartCheckpointButton->SetIsEnabled(true);
	
	UWorld* World = GetWorld();
	if (World)
	{
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}
	if (ReturnButton && !ReturnButton->OnClicked.IsBound())
	{
		ReturnButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	if (RestartLevelButton && !RestartLevelButton->OnClicked.IsBound())
	{
		RestartLevelButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::RestartLevelClicked);
	}
	if (RestartCheckpointButton && !RestartCheckpointButton->OnClicked.IsBound())
	{
		RestartCheckpointButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::RestartCheckpointClicked);
	}
	bool bIsCoopHost = false;
	
	// 1. Check if we are the Server/Host
	if (PlayerController && PlayerController->HasAuthority())
	{
		// 2. Check if the current Game Mode is the Co-op Game Mode!
		// (If we are in Arena Mode, this cast will safely fail)
		if (GetWorld() && GetWorld()->GetAuthGameMode<AGASPCoopGameMode>())
		{
			bIsCoopHost = true;
		}
	}

	// Hide 'Restart Level' from clients, but LEAVE 'Restart Checkpoint' visible 
	// so clients can individually respawn if the Game Mode allows it!
	if (!bIsCoopHost)
	{
		if (RestartLevelButton) RestartLevelButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	bool bShowRestartCheckpoint = false;
	bool bShowRestartLevel = false;

	if (PlayerController && PlayerController->HasAuthority())
	{
		if (AGASPCoopGameMode* GM = GetWorld()->GetAuthGameMode<AGASPCoopGameMode>())
		{
			if (GM->RespawnRule == ERespawnRule::SquadWipeOnly)
			{
				bShowRestartCheckpoint = true;
				bShowRestartLevel = true;
			}
			else if (GM->RespawnRule == ERespawnRule::IndividualRespawn)
			{
				bShowRestartCheckpoint = false;
				bShowRestartLevel = true;
			}
		}
	}
    
	if (RestartLevelButton) RestartLevelButton->SetVisibility(bShowRestartLevel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (RestartCheckpointButton) RestartCheckpointButton->SetVisibility(bShowRestartCheckpoint ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    
	UGameInstance* GameInstance = GetGameInstance();
	
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if (MultiplayerSessionsSubsystem)
		{
			MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UReturnToMainMenu::OnDestroySession);
		}
	}
}

bool UReturnToMainMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UReturnToMainMenu::OnDestroySession(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		ReturnButton->SetIsEnabled(true);
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		AGameModeBase* GameMode = World->GetAuthGameMode<AGameModeBase>();
		if (GameMode)
		{
			GameMode->ReturnToMainMenuHost();
		}
		else
		{
			PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
			if (PlayerController)
			{
				PlayerController->ClientReturnToMainMenuWithTextReason(FText());
			}
		}
	}
}

void UReturnToMainMenu::OnPlayerLeftGame()
{
	UE_LOG(LogTemp, Warning, TEXT("OnPlayerLeftGame()"))
	if (MultiplayerSessionsSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("MultiplayerSessionsSubsystem valid"))
		MultiplayerSessionsSubsystem->DestroySession();
	}
}

void UReturnToMainMenu::MenuTearDown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
	if (ReturnButton && ReturnButton->OnClicked.IsBound())
	{
		ReturnButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	if (RestartLevelButton && RestartLevelButton->OnClicked.IsBound())
	{
		RestartLevelButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::RestartLevelClicked);
	}
	if (RestartCheckpointButton && RestartCheckpointButton->OnClicked.IsBound())
	{
		RestartCheckpointButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::RestartCheckpointClicked);
	}
	if (MultiplayerSessionsSubsystem && MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.IsBound())
	{
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.RemoveDynamic(this, &UReturnToMainMenu::OnDestroySession);
	}
}

void UReturnToMainMenu::RestartLevelClicked()
{
	// Disable to prevent spamming the server
	if (RestartLevelButton) RestartLevelButton->SetIsEnabled(false);

	if (AGASPCorePlayerController* CorePC = Cast<AGASPCorePlayerController>(PlayerController))
	{
		CorePC->ServerRestartLevel();
		MenuTearDown(); // Close the menu
	}
}

void UReturnToMainMenu::RestartCheckpointClicked()
{
	// Disable to prevent spamming
	if (RestartCheckpointButton) RestartCheckpointButton->SetIsEnabled(false);

	if (AGASPCorePlayerController* CorePC = Cast<AGASPCorePlayerController>(PlayerController))
	{
		CorePC->ServerRestartCheckpoint();
		MenuTearDown(); // Close the menu
	}
}

void UReturnToMainMenu::ReturnButtonClicked()
{
	ReturnButton->SetIsEnabled(false);

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* FirstPlayerController = World->GetFirstPlayerController();
		if (FirstPlayerController)
		{
			AGASPPlayerCharacter* PlayerCharacter = Cast<AGASPPlayerCharacter>(FirstPlayerController->GetPawn());
			if (PlayerCharacter)
			{
				PlayerCharacter->ServerLeaveGame();
				PlayerCharacter->OnLeftGame.AddDynamic(this, &UReturnToMainMenu::OnPlayerLeftGame);
			}
			else
			{
				ReturnButton->SetIsEnabled(true);
			}
		}
	}
}