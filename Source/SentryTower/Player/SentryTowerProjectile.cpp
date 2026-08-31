// Copyright (c) 2024 Sentry. All Rights Reserved.

#include "SentryTowerProjectile.h"

#include "SentryTowerPawn.h"
#include "SentryTowerPlayerController.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SentryTower/Enemy/SentryTowerEnemyBase.h"

ASentryTowerProjectile::ASentryTowerProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));

	RootComponent = Body;
}

void ASentryTowerProjectile::Init()
{
	// Projectile follows the `TargetActor` enemy while it's moving if one was under cursor on shooting event
	// If `TargetActor` enemy dies before projectile reached it then the movement continues in last know direction
	// If there was no `TargetActor` enemy specified then the projectile simply moves to a `TargetLocation` point on the ground

	if (TargetToFollow != nullptr)
	{
		TargetToFollow->OnDestroyed.AddDynamic(this, &ASentryTowerProjectile::OnTargetDestroyed);
	}

	Direction = (TargetStationary - GetActorLocation()).GetSafeNormal();

	SetActorTickEnabled(true);
}

void ASentryTowerProjectile::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	auto PlayerController = Cast<ASentryTowerPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("ASentryTowerProjectile: Failed to get player controller!"));
		return;
	}

	auto TowerPawn = Cast<ASentryTowerPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!TowerPawn)
	{
		UE_LOG(LogTemp, Verbose, TEXT("ASentryTowerProjectile: Can't get tower pawn!"));
		return;
	}

	// apply tower level bonus to actual damage which's dealt to target
	float ResultingDamage = Damage + 0.1f * Damage * (TowerPawn->CurrentLevel - 1);

	if (Radius > 0.0f)
	{
		UGameplayStatics::ApplyRadialDamage(GetWorld(), ResultingDamage, GetActorLocation(), Radius, nullptr, { this, TowerPawn }, this);
	}
	else
	{
		auto EnemyTarget = Cast<ASentryTowerEnemyBase>(OtherActor);
		if(EnemyTarget != nullptr)
		{
			ApplySpecialEffect(OtherActor);
			UGameplayStatics::ApplyDamage(EnemyTarget, ResultingDamage, PlayerController, this, nullptr);
		}
	}

	UGameplayStatics::SpawnEmitterAtLocation(this, ParticleEffect, GetActorLocation());

	Destroy();
}

void ASentryTowerProjectile::OnTargetDestroyed(AActor* DestroyedEnemy)
{
	Direction = (DestroyedEnemy->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	TargetToFollow = nullptr;
}

void ASentryTowerProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);

	Body->OnComponentBeginOverlap.AddDynamic(this, &ASentryTowerProjectile::OnBeginOverlap);
}

void ASentryTowerProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TargetToFollow != nullptr)
	{
		FVector EnemyDirection = (TargetToFollow->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		FVector NewLocation = EnemyDirection * Speed * DeltaTime + GetActorLocation();
		SetActorLocation(NewLocation);
	}
	else
	{
		FVector NewLocation = Direction * Speed * DeltaTime + GetActorLocation();
		SetActorLocation(NewLocation);
	}
}

