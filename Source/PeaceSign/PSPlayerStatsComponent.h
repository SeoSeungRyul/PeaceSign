#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSPlayerStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPSStatsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPSHealthDepleted);

UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class PEACESIGN_API UPSPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSPlayerStatsComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadOnly, Category="Stats") float Health = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category="Stats") float MaxHealth = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category="Stats") float Stamina = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category="Stats") float MaxStamina = 100.0f;
	// Survival values are normalized placeholders until their balance rules are defined.
	UPROPERTY(BlueprintReadOnly, Category="Stats") float Hunger = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category="Stats") float MentalHealth = 100.0f;
	UPROPERTY(BlueprintAssignable, Category="Stats") FPSStatsChanged OnStatsChanged;
	UPROPERTY(BlueprintAssignable, Category="Stats") FPSHealthDepleted OnHealthDepleted;

	UFUNCTION(BlueprintCallable, Category="Stats") void ApplyDamage(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats") void RestoreHealth(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats") bool TryConsumeStamina(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stats") void SetSurvivalValues(float NewHunger, float NewMentalHealth);
	UFUNCTION(BlueprintCallable, Category="Stats") void SetActionActive(bool bActive);
	UFUNCTION(BlueprintCallable, Category="Stats") void SetMovementActive(bool bActive);
	UFUNCTION(BlueprintCallable, Category="Stats") void ResetStats();
private:
	bool bActionActive = false;
	bool bMovementActive = false;
	bool bConsumedSinceLastTick = false;
};
