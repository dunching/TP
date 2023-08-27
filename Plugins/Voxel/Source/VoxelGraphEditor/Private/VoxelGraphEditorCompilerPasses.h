// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraphEditorCompiler.h"

BEGIN_VOXEL_NAMESPACE(Graph)

struct FEditorCompilerPasses
{
	static bool RemoveDisabledNodes(FEditorCompiler& Compiler);
	static bool SetupPreview(FEditorCompiler& Compiler);
	static bool AddToBufferNodes(FEditorCompiler& Compiler);
	static bool RemoveSplitPins(FEditorCompiler& Compiler);
	static bool FixupLocalVariables(FEditorCompiler& Compiler);
};

END_VOXEL_NAMESPACE(Graph)