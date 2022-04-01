// Copyright Epic Games, Inc. All Rights Reserved.

#include "BrawlToDeathCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////////
// ABrawlToDeathCharacter

ABrawlToDeathCharacter::ABrawlToDeathCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 10.f;
	BaseLookUpRate = 10.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagMaxDistance = 130.0f;
	CameraBoom->CameraLagSpeed = 8.0f;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	SetReplicates(true);
	SetReplicateMovement(true);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	WalkSpeed = 150.f;
	RunSpeed = 300.f;
	SprintSpeed = 600.f;
}

//////////////////////////////////////////////////////////////////////////
// Input

void ABrawlToDeathCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ABrawlToDeathCharacter::SprintStarted);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ABrawlToDeathCharacter::SprintStopped);
	PlayerInputComponent->BindAction("ToggleRun", IE_Pressed, this, &ABrawlToDeathCharacter::ToggleRun);

	PlayerInputComponent->BindAction("Guarding", IE_Pressed, this, &ABrawlToDeathCharacter::GuardingStarted);
	PlayerInputComponent->BindAction("Guarding", IE_Released, this, &ABrawlToDeathCharacter::GuardingStopped);

	PlayerInputComponent->BindAction("Dodging", IE_Pressed, this, &ABrawlToDeathCharacter::DodgingFire);

	PlayerInputComponent->BindAction("ToggleCombatMode", IE_Pressed, this, &ABrawlToDeathCharacter::ToggleCombatMode);




	//For coordinating attacks
	PlayerInputComponent->BindAction("W", IE_Pressed, this, &ABrawlToDeathCharacter::IsWPressed);
	PlayerInputComponent->BindAction("W", IE_Released, this, &ABrawlToDeathCharacter::IsWReleased);

	PlayerInputComponent->BindAction("S", IE_Pressed, this, &ABrawlToDeathCharacter::IsSPressed);
	PlayerInputComponent->BindAction("S", IE_Released, this, &ABrawlToDeathCharacter::IsSReleased);

	PlayerInputComponent->BindAction("D", IE_Pressed, this, &ABrawlToDeathCharacter::IsDPressed);
	PlayerInputComponent->BindAction("D", IE_Released, this, &ABrawlToDeathCharacter::IsDReleased);

	PlayerInputComponent->BindAction("A", IE_Pressed, this, &ABrawlToDeathCharacter::IsAPressed);
	PlayerInputComponent->BindAction("A", IE_Released, this, &ABrawlToDeathCharacter::IsAReleased);

	//Attack inputs
	PlayerInputComponent->BindAction("LeftMouseAttack", IE_Pressed, this, &ABrawlToDeathCharacter::LeftMouseAttack);
	PlayerInputComponent->BindAction("RightMouseAttack", IE_Pressed, this, &ABrawlToDeathCharacter::RightMouseAttack);

	PlayerInputComponent->BindAxis("MoveForward", this, &ABrawlToDeathCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABrawlToDeathCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ABrawlToDeathCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &ABrawlToDeathCharacter::MouseX);

	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ABrawlToDeathCharacter::LookUpAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &ABrawlToDeathCharacter::MouseY);


	//// handle touch devices
	//PlayerInputComponent->BindTouch(IE_Pressed, this, &ABrawlToDeathCharacter::TouchStarted);
	//PlayerInputComponent->BindTouch(IE_Released, this, &ABrawlToDeathCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ABrawlToDeathCharacter::OnResetVR);
}

void ABrawlToDeathCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABrawlToDeathCharacter, WalkSpeed)
	DOREPLIFETIME(ABrawlToDeathCharacter, RunSpeed)
	DOREPLIFETIME(ABrawlToDeathCharacter, SprintSpeed)

}


void ABrawlToDeathCharacter::OnResetVR()
{
	// If BrawlToDeath is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in BrawlToDeath.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ABrawlToDeathCharacter::MouseX(float AxisValue)
{
	MouseXVal = AxisValue;
}

void ABrawlToDeathCharacter::MouseY(float AxisValue)
{
	MouseYVal = AxisValue;
}

void ABrawlToDeathCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ABrawlToDeathCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ABrawlToDeathCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ABrawlToDeathCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}






void ABrawlToDeathCharacter::ToggleRun()
{
	if (bIsToggleRun == true)
	{
		bIsToggleRun = false;
		Client_SetMaxWalkSpeed(WalkSpeed);

	}
	else if (bIsToggleRun == false)
	{
		bIsToggleRun = true;
		Client_SetMaxWalkSpeed(RunSpeed);

	}
}



void ABrawlToDeathCharacter::SprintFalseTimer()
{
	bIsSprinting = false;
}


void ABrawlToDeathCharacter::SprintStarted()
{
	//If combat mode is on it only gets on for a sec so the player cannot dashes constantly
	if (bIsCombatMode == true)
	{
		bIsSprinting = true;

		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ABrawlToDeathCharacter::SprintFalseTimer, 0.5f, false);

	}
	else if (bIsCombatMode == false)
	{
		bIsSprinting = true;
		Client_SetMaxWalkSpeed(SprintSpeed);
	}
}




void ABrawlToDeathCharacter::SprintStopped()
{
	bIsSprinting = false;

	if (bIsToggleRun == true)
	{
		Client_SetMaxWalkSpeed(RunSpeed);
	}
	else if (bIsToggleRun == false)
	{
		Client_SetMaxWalkSpeed(WalkSpeed);
	}
	

}






void ABrawlToDeathCharacter::ToggleCombatMode()
{
	if (bIsCombatMode == true)
	{
		bIsCombatMode = false;
	}
	else if (bIsCombatMode == false)
	{
		bIsCombatMode = true;
	}
}




void ABrawlToDeathCharacter::IsWPressed()
{
	bIsW = true;
	bIsKicking = false;
	bIsPunching = false;
}

void ABrawlToDeathCharacter::IsWReleased()
{
	bIsW = false;
	bIsKicking = false;
	bIsPunching = false;
}

void ABrawlToDeathCharacter::IsSPressed()
{
	bIsS = true;
	bIsKicking = false;
	bIsPunching = false;
}

void ABrawlToDeathCharacter::IsSReleased()
{
	bIsS = false;
	bIsKicking = false;
	bIsPunching = false;
}

void ABrawlToDeathCharacter::IsDPressed()
{
	bIsD = true;
	bIsKicking = false;
	bIsPunching = false;
}

void ABrawlToDeathCharacter::IsDReleased()
{
	bIsD = false;
	bIsKicking = false;
	bIsPunching = false;

}

void ABrawlToDeathCharacter::IsAPressed()
{
	bIsA = true;
	bIsKicking = false;
	bIsPunching = false;
}

void ABrawlToDeathCharacter::IsAReleased()
{
	bIsA = false;
	bIsKicking = false;
	bIsPunching = false;
}


/**  ServerSide Functions*/


void ABrawlToDeathCharacter::Client_SetMaxWalkSpeed_Implementation(float Speed)
{
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	Server_SetMaxWalkSpeed(GetCharacterMovement()->MaxWalkSpeed);
}

void ABrawlToDeathCharacter::Server_SetMaxWalkSpeed_Implementation(float Speed)
{
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}


/// <summary>
/// 
/// 
/// *********************************************************CombatMode Functions*********************************************************
/// 
/// 
/// </summary>



/// 
/// LeftMouse Click Or LeftMouse Click + W -> Jab
/// LeftMouse Click + A Or LeftMouse Click + W + A Or LeftMouse Click + S + A  -> LeftHook
/// LeftMouse Click + D Or LeftMouse Click + W + D Or LeftMouse Click + S + D  -> RightHook
/// LeftMouse Click + Mouse Forward -> Straight
/// LeftMouse Click + S -> UpperCut
/// 

void ABrawlToDeathCharacter::LeftMouseAttack()
{
	bIsUpper = true;

	if (((bIsA == true && bIsW == true) || (bIsA == true && bIsS == true) || bIsA == true) && (bIsCombatMode == true) && (bIsPunching == false) && (bIsDodging == false))
	{
		bIsPunching = true;
		bIsLeftAttack = true;
		DamageDealt = 8.0;

		if (LeftMouseLeftHook)
		{
			PlayAnimMontage(LeftMouseLeftHook, 1.3f, NAME_None);
		}
	}
	else if (((bIsD == true && bIsW == true) || (bIsD == true && bIsS == true) || bIsD == true) && (bIsCombatMode == true) && (bIsPunching == false) && (bIsDodging == false))
	{
		bIsPunching = true;
		DamageDealt = 8.0f;
		if (LeftMouseRightHook)
		{
			PlayAnimMontage(LeftMouseRightHook, 1.3f, NAME_None);
		}
	}
	else if ((MouseYVal < -0.15) && (bIsCombatMode == true) && (bIsPunching == false) && (bIsDodging == false))
	{
		bIsPunching = true;
		DamageDealt = 10.0f;
		if (LeftMouseStraight)
		{
			PlayAnimMontage(LeftMouseStraight, 1.3f, NAME_None);

		}

	}
	else if ((MouseYVal > 0.05) && (bIsCombatMode == true) && (bIsPunching == false) && (bIsDodging == false))
	{
		bIsPunching = true;
		bIsLeftAttack = true;
		DamageDealt = 15.0f;
		if (LeftMouseUpperCut)
		{
			PlayAnimMontage(LeftMouseUpperCut, 1.3f, NAME_None);
		}
	}
	else if ((bIsCombatMode == true) && (bIsPunching == false) && (bIsDodging == false))
	{
		{
			bIsPunching = true;
			bIsLeftAttack = true;
			DamageDealt = 5.0f;
			if (LeftMouseJab)
			{
				PlayAnimMontage(LeftMouseJab, 1.5f, NAME_None);
			}
		}
	}
	if ((bIsDodging == false) && (bHasDodged == false))
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [&]()
			{
				bIsUpper = true; bIsPunching = false; bIsLeftAttack = false;
			}, 0.5f, false);
	}
}

void ABrawlToDeathCharacter::RightMouseAttack()
{
	bIsUpper = false;
	if (((bIsA == true && bIsW == true) || bIsA == true) && (bIsCombatMode == true) && (bIsKicking == false) && (bIsDodging == false))
	{

		bIsKicking = true;
		bIsLeftAttack = true;
		DamageDealt = 10.0f;
		if (RightMouseLeftMiddleKick)
		{
			PlayAnimMontage(RightMouseLeftMiddleKick, 1.3f, NAME_None);
		}
	}
	else if (((bIsD == true && bIsW == true) || bIsD == true) && (bIsCombatMode == true) && (bIsKicking == false) && (bIsDodging == false))
	{
		bIsKicking = true;
		DamageDealt = 10.0f;
		if (RightMouseRightMiddleKick)
		{
			PlayAnimMontage(RightMouseRightMiddleKick, 1.3f, NAME_None);
		}
	}
	else if ((MouseYVal < -0.05) && (bIsCombatMode == true) && (bIsKicking == false) && (bIsDodging == false))
	{

		bIsKicking = true;
		DamageDealt = 20.0f;
		if (RightMouseHighKick)
		{
			PlayAnimMontage(RightMouseHighKick, 1.3f, NAME_None);
		}
	}
	else if (((bIsCombatMode == true) && (bIsKicking == false)) || (((bIsCombatMode == true) && (bIsKicking == false)) && bIsW == true) && (bIsDodging == false))
	{
		{
			bIsKicking = true;
			DamageDealt = 5.0f;
			if (RightMouseLowKick)
			{
				PlayAnimMontage(RightMouseLowKick, 1.5f, NAME_None);
			}
		}
	}

	if (bIsDodging == false && bHasDodged == false)
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle,
			[&]()
			{
				bIsUpper = true; bIsKicking = false; bIsLeftAttack = false;
			},
			1.1f,
				false);
	}
}




void ABrawlToDeathCharacter::PivotAnimationsController()
{
	if (bIsCombatMode == true && bIsKicking == false && bIsPunching == false)
	{
		if (MouseXVal > 1.0f)
		{
			bIsUpper = false;
			if (RightPivot)
			{
				PlayAnimMontage(RightPivot, 1.4f, NAME_None);
			}
		}
		else if (MouseXVal < -1.0f)
		{
			bIsUpper = false;
			if (LeftPivot)
			{
				PlayAnimMontage(LeftPivot, 1.4f, NAME_None);
			}
		}
	}

	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [&]()
		{
			bIsUpper = true;
		}, 1.f, false);
}



void ABrawlToDeathCharacter::GuardingStarted()
{
	if (bIsKicking == false && bIsCombatMode == true)
	{
		DamageReducingValue = 3.f;
		bIsUpper = true;
		bIsGuarding = true;
	}
}


void ABrawlToDeathCharacter::GuardingStopped()
{
	StopAnimMontage(Guarding);
	DamageReducingValue = 1.f;
	bIsGuarding = false;
}


void ABrawlToDeathCharacter::DodgingFire()
{
	if ((bIsCombatMode == true && bIsKicking == false && bIsPunching == false)
		&& ((bIsD == true) || (bIsD == true) && (bIsW == true) || (bIsD == true) && (bIsS == true) || bIsW == true))
	{
		bIsUpper = true;
		bIsDodging = true;
		DamageMultiplier = 2.f;
		if (DodgingRight)
		{
			PlayAnimMontage(DodgingRight, 1.f, NAME_None);
		}
		bHasDodged = true;
	}
	else if (bIsCombatMode == true && bIsKicking == false && bIsPunching == false)
	{
		bIsUpper = true;
		bIsDodging = true;
		DamageMultiplier = 2.f;
		if (DodgingLeft)
		{
			PlayAnimMontage(DodgingLeft, 1.f, NAME_None);
		}
		bHasDodged = true;
	}

	//Turns bIsDodging off as soon as the animation is over 
	//and then Turns off bHasDodged off and sets DamageMultiplier back to 1
	//a second after the animation is over

	GetWorld()->GetTimerManager().SetTimer(TimerHandle,
		[&]()
		{
			bIsDodging = false; GetWorld()->GetTimerManager().SetTimer(TimerHandle,
				[&]()
				{
					bHasDodged = false;  DamageMultiplier = 1.f;
				},
				1.f,
					false);
		},
		1.5f,
			false);



}


