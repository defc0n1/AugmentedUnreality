/*
Copyright 2016 Krzysztof Lis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "AugmentedUnreality.h"
#include "AURMarkerComponentBase.h"
#include "AUROpenCV.h"
#include <vector>

const float UAURMarkerComponentBase::MARKER_DEFAULT_SIZE = 10.0;
const float UAURMarkerComponentBase::MARKER_TEXT_RELATIVE_SCALE = 0.5;

/**
The order in ArUco is top-left, top-right, bottom-right, bottom-left,
but since OpenCV's vectors have different handedness, we need to have different order here.
The change swaps X with Y, so top-left with bottom-right:
bottom-right, top-right, top-left, bottom-left
**/
const std::vector<FVector> UAURMarkerComponentBase::LOCAL_CORNERS{
	FVector(0.5, -0.5, 0.0),
	FVector(0.5, 0.5, 0.0),
	FVector(-0.5, 0.5, 0.0),
	FVector(-0.5, -0.5, 0.0)
};

UAURMarkerComponentBase::UAURMarkerComponentBase()
	: Id(1)
	, BoardSizeCm(MARKER_DEFAULT_SIZE)
	, MarginCm(1.0)
{
	bWantsInitializeComponent = true;

	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MarkerText = CreateDefaultSubobject<UTextRenderComponent>("MarkerText");
	MarkerText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerText->SetAbsolute(false, false, false);

	MarkerText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	MarkerText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	MarkerText->SetTextRenderColor(FColor(50, 200, 50));
}

void UAURMarkerComponentBase::InitializeComponent()
{
	Super::InitializeComponent();

	if (MarkerText->GetAttachParent() != this)
	{
		MarkerText->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform, "Text");
	}
	MarkerText->SetRelativeRotation(FRotator(90.0f, 0.0f, 180.0f));
	MarkerText->SetRelativeScale3D(FVector(1, 1, 1) * 0.25);
	MarkerText->SetRelativeLocation(FVector(0, 0, 0.1));

	SetBoardSize(BoardSizeCm);
	SetId(Id);
}

FMarkerDefinitionData UAURMarkerComponentBase::GetDefinition() const
{
	//	UE_LOG(LogAUR, Log, TEXT("Marker %d (%s)"), Id, *this->GetName());

	// Determine the transform from board actor to this component
	FTransform transform_this_to_actor = this->GetRelativeTransform();

	//TArray<USceneComponent*> parent_components;
	//this->GetParentComponents(parent_components);

	USceneComponent* root_component = GetAttachmentRoot();
	USceneComponent* parent_comp = GetAttachParent();

	while (parent_comp && parent_comp != root_component)
	{
		// Runtime/Core/Public/Math/Transform.h:
		// C = A * B will yield a transform C that logically first applies A then B to any subsequent transformation.
		// (we want the parent-most transform done first)
		transform_this_to_actor *= parent_comp->GetRelativeTransform();
		parent_comp = parent_comp->GetAttachParent();
	}

	// Transform the corners
	FMarkerDefinitionData def_data(Id);

	// Don't multiply by this marker's size becuase the transform's scale will do it.
	// But calculate what part of the board is taken by the margin
	const float pattern_size_relative = (BoardSizeCm - 2 * MarginCm) * MARKER_DEFAULT_SIZE/BoardSizeCm;

	//UE_LOG(LogAUR, Log, TEXT("Marker %d"), Id);

	for (int idx = 0; idx < 4; idx++)
	{
		def_data.Corners[idx] = transform_this_to_actor.TransformPosition(LOCAL_CORNERS[idx] * pattern_size_relative);
		//UE_LOG(LogAUR, Log, TEXT("	%s"), *def_data.Corners[idx].ToString());
	}

	return def_data;
}

void UAURMarkerComponentBase::AppendToBoardDefinition(cv::aur::ArucoBoardDefinition& board_def)
{
	FTransform transform_this_to_actor = this->GetRelativeTransform();

	USceneComponent* root_component = GetAttachmentRoot();
	USceneComponent* parent_comp = GetAttachParent();

	while (parent_comp && parent_comp != root_component)
	{
		// Runtime/Core/Public/Math/Transform.h:
		// C = A * B will yield a transform C that logically first applies A then B to any subsequent transformation.
		// (we want the parent-most transform done first)
		transform_this_to_actor *= parent_comp->GetRelativeTransform();
		parent_comp = parent_comp->GetAttachParent();
	}

	// Transform the corners
	FMarkerDefinitionData def_data(Id);

	// Don't multiply by this marker's size becuase the transform's scale will do it.
	// But calculate what part of the board is taken by the margin
	const float pattern_size_relative = (BoardSizeCm - 2 * MarginCm) * MARKER_DEFAULT_SIZE/BoardSizeCm;

	//UE_LOG(LogAUR, Log, TEXT("Marker %d"), Id);

	std::vector<cv::Point3f> raw_corners(4);

	for (int idx = 0; idx < 4; idx++)
	{
		raw_corners[idx] = FAUROpenCV::ConvertUnrealVectorToOpenCvPoint(
			transform_this_to_actor.TransformPosition(LOCAL_CORNERS[idx] * pattern_size_relative)
		);
	}

	board_def.AddMarker(Id, raw_corners);
}

void UAURMarkerComponentBase::SetId(int32 new_id)
{
	Id = new_id;
	if (MarkerText)
	{
		MarkerText->SetText(FText::Format(NSLOCTEXT("AUR", "MarkerEditorText", "{0}"), FText::AsNumber(Id)));
	}
}

void UAURMarkerComponentBase::SetBoardSize(float new_board_size)
{
	BoardSizeCm = new_board_size;

	const float scale = BoardSizeCm / MARKER_DEFAULT_SIZE;
	this->SetWorldScale3D(FVector(scale, scale, scale));
}

#if WITH_EDITOR
void UAURMarkerComponentBase::PostEditChangeProperty(FPropertyChangedEvent & property_change_event)
{
	Super::PostEditChangeProperty(property_change_event);

	const FName property_name = (property_change_event.Property != nullptr) ? property_change_event.Property->GetFName() : NAME_None;

	//UE_LOG(LogAUR, Warning, TEXT("UAURMarkerComponentBase::PostEditChangeProperty %s"), *property_name.ToString());

	if (property_name == GET_MEMBER_NAME_CHECKED(UAURMarkerComponentBase, Id))
	{
		SetId(Id);
	}
	else if (property_name == GET_MEMBER_NAME_CHECKED(UAURMarkerComponentBase, BoardSizeCm))
	{
		SetBoardSize(BoardSizeCm);
	}
}
#endif
