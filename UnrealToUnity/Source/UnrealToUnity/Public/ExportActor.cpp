// Copyright Ciprian Stanciu 2024

#include "ExportActor.h"
#include "ExportToUnity.h"
#include "EngineUtils.h"
#include "LandscapeGizmoActiveActor.h"
#include "Editor.h"

// Sets default values
AExportActor::AExportActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AExportActor::BeginPlay()
{
	Super::BeginPlay();
	
	EventExport();
}

// Called every frame
void AExportActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
FString GetDefaultOutputFolder();
void AExportActor::EventExport()
{
	FUnrealToXModule::DoExport( );
	//DoExport( OutputFolder );
}
bool DoRotateOnExport = true;
extern TAutoConsoleVariable<int> CVarFBXPerActor;
void AExportActor::DoExport()
{
	DetachAllActors();

	if ( DoRotateOnExport )
		RotateActorsForExport( true );

	UWorld* world = GEditor->GetEditorWorldContext( false ).World();
	DoExportScene( world );

	if ( DoRotateOnExport )
		RotateActorsForExport( false );
}
void AExportActor::DetachAllActors()
{
	UWorld* world = GEditor->GetEditorWorldContext( false ).World();

	for( TActorIterator<AActor> ActorItr( world ); ActorItr; ++ActorItr )
	{
		AActor* Actor = *ActorItr;
		checkSlow( Actor );

		USceneComponent* RootComp = Actor->GetRootComponent();
		if( RootComp != nullptr && RootComp->GetAttachParent() != nullptr )
		{
			AActor* OldParentActor = RootComp->GetAttachParent()->GetOwner();
			OldParentActor->Modify();
			RootComp->DetachFromComponent( FDetachmentTransformRules::KeepWorldTransform );
			Actor->SetFolderPath_Recursively( OldParentActor->GetFolderPath() );
		}
	}
}
void AExportActor::RotateActorsForExport( bool Forward )
{

	UWorld* world = GEditor->GetEditorWorldContext( false ).World();

	GEditor->SelectNone( false, true, false );
	
	for( TActorIterator<AActor> ActorItr( world ); ActorItr; ++ActorItr )
	{
		AActor* actor = *ActorItr;

		const AActor* Parent = actor->GetParentActor();
		ALandscapeGizmoActiveActor* GizmoActor = Cast< ALandscapeGizmoActiveActor>( actor );
		if( GizmoActor )//crashes on 5.2
			continue;
		//Don't modify children
		if( Parent )
			continue;

		//Prevent construction scripts from messing up transforms in some cases!
		actor->bActorSeamlessTraveled = 1;
		
		FVector InDeltaDrag(0,0,0);	
		float XRot = 0;
		float YRot = 0;
		float ZRot = 90.0f;
		if( !Forward )
		{
			XRot *= -1;
			YRot *= -1;
			ZRot *= -1;
		}
		FRotator InDeltaRot( XRot, YRot, ZRot );
		FVector InDeltaScale( 0, 0, 0 );
		FVector ModifiedDeltaScale = InDeltaScale;
		GEditor->ApplyDeltaToActor(
			actor,
			true,
			&InDeltaDrag,
			&InDeltaRot,
			&ModifiedDeltaScale,
			false,//IsAltPressed(),
			false,//IsShiftPressed(),
			false//IsCtrlPressed() 
			);
	}
}

