// Copyright Ciprian Stanciu 2024

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <string>

#include "UTXModule.h"

#include "ExportActor.generated.h"

UCLASS()
class AExportActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AExportActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	

	UPROPERTY( EditAnywhere, Category = "Settings" )
	RenderPipeline Pipeline = RenderPipeline::RP_BUILTIN;

	UPROPERTY( EditAnywhere, Category = "Settings" )
	FString OutputFolder = TEXT( "C:\\UnrealToUnity\\" );

	UFUNCTION( BlueprintCallable, Category = "Settings" )
	void EventExport();

	static void DoExport( );
	static void RotateActorsForExport( bool Forward );
	static void DetachAllActors();

	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
