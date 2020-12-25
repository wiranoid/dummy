inline joint_pose
Lerp(joint_pose *From, f32 t, joint_pose *To)
{
    joint_pose Result;

    Result.Translation = Lerp(From->Translation, t, To->Translation);
    Result.Rotation = Slerp(From->Rotation, t, To->Rotation);
    // todo: logarithmic interpolation for scale? (https://www.cmu.edu/biolphys/deserno/pdf/log_interpol.pdf)
    Result.Scale = Lerp(From->Scale, t, To->Scale);

    return Result;
}

#if 0
inline joint_pose
Lerp(joint_pose *A, joint_pose *B, joint_pose *C, f32 Alpha, f32 Beta, f32 Gamma)
{
    joint_pose Result;

    Result.Translation = A->Translation * Alpha + B->Translation * Beta + C->Translation * Gamma;
    // todo: probably wrong. Use slerp instead
    Result.Rotation = A->Rotation * Alpha + B->Rotation * Beta + C->Rotation * Gamma;
    Result.Scale = A->Scale * Alpha + B->Scale * Beta + C->Scale * Gamma;

    return Result;
}
#endif

internal void
Lerp(skeleton_pose *From, f32 t, skeleton_pose *To, skeleton_pose *Dest)
{
    u32 JointCount = From->Skeleton->JointCount;

    Assert(To->Skeleton->JointCount == JointCount);
    Assert(Dest->Skeleton->JointCount == JointCount);

    for (u32 JointIndex = 0; JointIndex < JointCount; ++JointIndex)
    {
        joint_pose *FromPose = From->LocalJointPoses + JointIndex;
        joint_pose *ToPose = To->LocalJointPoses + JointIndex;
        joint_pose *DestPose = Dest->LocalJointPoses + JointIndex;

        *DestPose = Lerp(FromPose, t, ToPose);
    }
}

internal mat4
CalculateGlobalJointPose(joint *CurrentJoint, joint_pose *CurrentJointPose, skeleton_pose *Pose)
{
    mat4 Result = mat4(1.f);

    while (true)
    {
        mat4 Global = Transform(*CurrentJointPose);
        Result = Global * Result;

        if (CurrentJoint->ParentIndex == -1)
        {
            break;
        }

        CurrentJointPose = Pose->LocalJointPoses + CurrentJoint->ParentIndex;
        CurrentJoint = Pose->Skeleton->Joints + CurrentJoint->ParentIndex;
    }

    return Result;
}

// todo: optimize (caching?)
inline animation_sample *
GetAnimationSampleByJointIndex(animation_clip *Animation, u32 JointIndex)
{
    animation_sample *Result = 0;

    for (u32 AnimationSampleIndex = 0; AnimationSampleIndex < Animation->PoseSampleCount; ++AnimationSampleIndex)
    {
        animation_sample *AnimationSample = Animation->PoseSamples + AnimationSampleIndex;

        if (AnimationSample->JointIndex == JointIndex)
        {
            Result = AnimationSample;
            break;
        }
    }

    return Result;
}

struct closest_key_frames_result
{
    key_frame *Current;
    key_frame *Next;
};

internal closest_key_frames_result
FindClosestKeyFrames(animation_sample *PoseSample, f32 CurrentTime)
{
    Assert(PoseSample->KeyFrameCount > 1);

    closest_key_frames_result Result = {};

    for (u32 KeyFrameIndex = 0; KeyFrameIndex < PoseSample->KeyFrameCount - 1; ++KeyFrameIndex)
    {
        key_frame *CurrentKeyFrame = PoseSample->KeyFrames + KeyFrameIndex;
        key_frame *NextKeyFrame = PoseSample->KeyFrames + KeyFrameIndex + 1;

        if (CurrentKeyFrame->Time <= CurrentTime && CurrentTime < NextKeyFrame->Time)
        {
            Result.Current = CurrentKeyFrame;
            Result.Next = NextKeyFrame;
            break;
        }
    }

    // todo: ?
    if (!Result.Current || !Result.Next)
    {
        Result.Current = PoseSample->KeyFrames + (PoseSample->KeyFrameCount - 1);
        Result.Next = PoseSample->KeyFrames + 0;
    }

    return Result;
}

internal void
AnimateSkeletonPose(skeleton_pose *SkeletonPose, animation_clip *Animation, f32 CurrentTime)
{
    for (u32 JointIndex = 0; JointIndex < SkeletonPose->Skeleton->JointCount; ++JointIndex)
    {
        joint_pose *LocalJointPose = SkeletonPose->LocalJointPoses + JointIndex;

        animation_sample *PoseSample = GetAnimationSampleByJointIndex(Animation, JointIndex);

        if (PoseSample && PoseSample->KeyFrameCount > 1)
        {
            closest_key_frames_result ClosestKeyFrames = FindClosestKeyFrames(PoseSample, CurrentTime);
            key_frame *CurrentKeyFrame = ClosestKeyFrames.Current;
            key_frame *NextKeyFrame = ClosestKeyFrames.Next;

            f32 t = (CurrentTime - CurrentKeyFrame->Time) / (Abs(NextKeyFrame->Time - CurrentKeyFrame->Time));

            Assert(t >= 0.f && t <= 1.f);

            *LocalJointPose = Lerp(&CurrentKeyFrame->Pose, t, &NextKeyFrame->Pose);
        }
    }

    if (Animation->InPlace)
    {
        joint_pose *RootTranslationLocalJointPose = GetRootTranslationLocalJointPose(SkeletonPose);
        RootTranslationLocalJointPose->Translation = vec3(0.f, RootTranslationLocalJointPose->Translation.y, 0.f);
    }
}

inline animation_node *
GetActiveNode(animation_graph *Graph)
{
    animation_node *Result = Graph->Nodes + Graph->ActiveNodeIndex;
    return Result;
}

inline void
ResetAnimationState(animation_state *Animation)
{
    Animation->Time = 0.f;
}

// todo: similar functions (enable/disable?)
inline void
EnableAnimationNode(animation_node *Node)
{
    Node->Weight = 1.f;

    switch (Node->Type)
    {
        case AnimationNodeType_SingleMotion:
        {
            ResetAnimationState(&Node->Animation);

            break;
        }
        case AnimationNodeType_BlendSpace:
        {
            for (u32 ValueIndex = 0; ValueIndex < Node->BlendSpace->BlendSpaceValueCount; ++ValueIndex)
            {
                blend_space_1d_value *Value = Node->BlendSpace->BlendSpaceValues;

                ResetAnimationState(&Value->AnimationState);
            }

            break;
        }
        case AnimationNodeType_Graph:
        {
            EnableAnimationNode(Node->Graph->Entry);

            Node->Graph->Entry->Weight = 1.f;
            Node->Graph->ActiveNodeIndex = Node->Graph->Entry->Index;

            break;
        }
    }
}

internal void
DisableAnimationNode(animation_node *Node)
{
    Node->Weight = 0.f;

    switch (Node->Type)
    {
        case AnimationNodeType_SingleMotion:
        {
            ResetAnimationState(&Node->Animation);

            break;
        }
        case AnimationNodeType_BlendSpace:
        {
            for (u32 ValueIndex = 0; ValueIndex < Node->BlendSpace->BlendSpaceValueCount; ++ValueIndex)
            {
                blend_space_1d_value *Value = Node->BlendSpace->BlendSpaceValues + ValueIndex;

                Value->Weight = 0.f;
                ResetAnimationState(&Value->AnimationState);
            }

            break;
        }
        case AnimationNodeType_Graph:
        {
            for (u32 SubNodeIndex = 0; SubNodeIndex < Node->Graph->NodeCount; ++SubNodeIndex)
            {
                animation_node *SubNode = Node->Graph->Nodes + SubNodeIndex;
                DisableAnimationNode(SubNode);
            }

            Node->Graph->Entry->Weight = 1.f;
            Node->Graph->ActiveNodeIndex = Node->Graph->Entry->Index;

            break;
        }
    }
}

internal void
CrossFade(animation_mixer *Mixer, animation_node *From, animation_node *To, f32 Duration)
{
    Mixer->From = From;
    Mixer->To = To;
    Mixer->Duration = Duration;
    Mixer->Time = 0.f;
    Mixer->StartWeight = To->Weight;

    // todo: ?
    //FadeIn(Mixer, To, Duration);
    //FadeOut(Mixer, From, Duration);
}

internal void
AnimationMixerPerFrameUpdate(animation_mixer *Mixer, f32 Delta)
{
    if (Mixer->From && Mixer->To)
    {
        Mixer->Time += Delta;

        if (Mixer->Time > Mixer->Duration)
        {
            Mixer->Time = Mixer->Duration;
        }

        f32 Value = Mixer->Time / Mixer->Duration;

        f32 FromWeight = Max(0.f, (1.f - Mixer->StartWeight) - Value);
        Assert(FromWeight >= 0.f && FromWeight <= 1.f);
        Mixer->From->Weight = FromWeight;

        f32 ToWeight = Min(1.f, Mixer->StartWeight + Value);
        Assert(ToWeight >= 0.f && ToWeight <= 1.f);
        Mixer->To->Weight = ToWeight;

        if (Mixer->Time == Mixer->Duration)
        {
            Assert(Mixer->From->Weight == 0.f);
            Assert(Mixer->To->Weight == 1.f);

            DisableAnimationNode(Mixer->From);

            // don't need to do this 
            //EnableAnimationNode(Mixer->To);

            Mixer->From = 0;
            Mixer->To = 0;
        }
    }
}

internal void
UpdateGlobalJointPoses(skeleton_pose *Pose, transform Transform)
{
    joint_pose *RootLocalJointPose = GetRootLocalJointPose(Pose);
    RootLocalJointPose->Translation = Transform.Translation;
    RootLocalJointPose->Rotation = Transform.Rotation;
    RootLocalJointPose->Scale = Transform.Scale;

    for (u32 JointIndex = 0; JointIndex < Pose->Skeleton->JointCount; ++JointIndex)
    {
        joint *Joint = Pose->Skeleton->Joints + JointIndex;
        joint_pose *LocalJointPose = Pose->LocalJointPoses + JointIndex;
        mat4 *GlobalJointPose = Pose->GlobalJointPoses + JointIndex;

        *GlobalJointPose = CalculateGlobalJointPose(Joint, LocalJointPose, Pose);
    }
}

#if 0
internal void
TriangularLerpBlend(
    blend_space_2d_triangle *AnimationBlendSpaceTriangle,
    vec2 Blend,
    skeleton_pose *Pose,
    memory_arena *Arena
)
{
    scoped_memory ScopedMemory(Arena);
    skeleton_pose *SkeletonPoses = PushArray(ScopedMemory.Arena, 3, skeleton_pose);

    for (u32 SkeletonPoseIndex = 0; SkeletonPoseIndex < 3; ++SkeletonPoseIndex)
    {
        skeleton_pose *SkeletonPose = SkeletonPoses + SkeletonPoseIndex;
        SkeletonPose->Skeleton = Pose->Skeleton;
        SkeletonPose->LocalJointPoses = PushArray(ScopedMemory.Arena, Pose->Skeleton->JointCount, joint_pose);

        for (u32 JointIndex = 0; JointIndex < Pose->Skeleton->JointCount; ++JointIndex)
        {
            joint_pose *LocalJointPose = SkeletonPose->LocalJointPoses + JointIndex;
            joint_pose *SkeletonLocalJointPose = Pose->LocalJointPoses + JointIndex;

            *LocalJointPose = *SkeletonLocalJointPose;
        }
    }

    skeleton_pose *SkeletonPoseA = SkeletonPoses + 0;
    skeleton_pose *SkeletonPoseB = SkeletonPoses + 1;
    skeleton_pose *SkeletonPoseC = SkeletonPoses + 2;

    blend_space_2d_value *A = &AnimationBlendSpaceTriangle->Points[0];
    blend_space_2d_value *B = &AnimationBlendSpaceTriangle->Points[1];
    blend_space_2d_value *C = &AnimationBlendSpaceTriangle->Points[2];

    AnimateSkeletonPose(SkeletonPoseA, A->AnimationState->Clip, A->AnimationState->Time);
    AnimateSkeletonPose(SkeletonPoseB, B->AnimationState->Clip, B->AnimationState->Time);
    AnimateSkeletonPose(SkeletonPoseC, C->AnimationState->Clip, C->AnimationState->Time);

    f32 Alpha, Beta, Gamma;
    Barycentric(Blend, A->Value, B->Value, C->Value, Alpha, Beta, Gamma);

    Assert(Abs(1.f - (Alpha + Beta + Gamma)) < 0.0001f);

    for (u32 JointIndex = 0; JointIndex < Pose->Skeleton->JointCount; ++JointIndex)
    {
        joint_pose *LocalJointPose = Pose->LocalJointPoses + JointIndex;

        joint_pose *LocalJointPoseA = SkeletonPoseA->LocalJointPoses + JointIndex;
        joint_pose *LocalJointPoseB = SkeletonPoseB->LocalJointPoses + JointIndex;
        joint_pose *LocalJointPoseC = SkeletonPoseC->LocalJointPoses + JointIndex;

        *LocalJointPose = Lerp(LocalJointPoseA, LocalJointPoseB, LocalJointPoseC, Alpha, Beta, Gamma);
    }
}
#endif

internal void
TransitionToNode(animation_graph *Graph, const char *NodeName)
{
    animation_node *FromNode = GetActiveNode(Graph);
    animation_transition *Transition = 0;

    for (u32 TransitionIndex = 0; TransitionIndex < FromNode->TransitionCount; ++TransitionIndex)
    {
        animation_transition *CurrentTransition = FromNode->Transitions + TransitionIndex;

        if (StringEquals(CurrentTransition->To->Name, NodeName))
        {
            Transition = CurrentTransition;
            break;
        }
    }

    Assert(Transition);

    animation_node *ToNode = Transition->To;

    switch (Transition->Type)
    {
        case AnimationTransitionType_Immediate:
        {
            Graph->ActiveNodeIndex = ToNode->Index;

            DisableAnimationNode(FromNode);
            EnableAnimationNode(ToNode);

            break;
        }
        case AnimationTransitionType_Crossfade:
        {
            Graph->ActiveNodeIndex = ToNode->Index;

            CrossFade(&Graph->Mixer, FromNode, ToNode, Transition->Duration);

            break;
        }
        case AnimationTransitionType_Transitional:
        {
            Graph->ActiveNodeIndex = Transition->TransitionNode->Index;

            CrossFade(&Graph->Mixer, FromNode, Transition->TransitionNode, Transition->Duration);

            break;
        }
        default:
        {
            Assert(!"Invalid Transition Type");
        }
    }
}

internal void
AnimationStatePerFrameUpdate(animation_state *AnimationState, f32 Delta)
{
    AnimationState->Time += Delta;

    if (AnimationState->Time > AnimationState->Clip->Duration)
    {
        if (AnimationState->Clip->IsLooping)
        {
            AnimationState->Time = 0.f;
        }
        else
        {
            // todo: ?
            //AnimationState->Time = 0.f;
        }
    }
}

internal void
LinearLerpBlend(blend_space_1d *BlendSpace, f32 BlendParameter)
{
    blend_space_1d_value *BlendValueFrom = 0;
    blend_space_1d_value *BlendValueTo = 0;

    for (u32 AnimationBlendValueIndex = 0; AnimationBlendValueIndex < BlendSpace->BlendSpaceValueCount - 1; ++AnimationBlendValueIndex)
    {
        blend_space_1d_value *CurrentAnimationBlendValue = BlendSpace->BlendSpaceValues + AnimationBlendValueIndex;
        blend_space_1d_value *NextAnimationBlendValue = BlendSpace->BlendSpaceValues + AnimationBlendValueIndex + 1;

        if (InRange(BlendParameter, CurrentAnimationBlendValue->Value, NextAnimationBlendValue->Value))
        {
            BlendValueFrom = CurrentAnimationBlendValue;
            BlendValueTo = NextAnimationBlendValue;
            break;
        }
    }

    for (u32 AnimationBlendValueIndex = 0; AnimationBlendValueIndex < BlendSpace->BlendSpaceValueCount; ++AnimationBlendValueIndex)
    {
        blend_space_1d_value *AnimationBlendValue = BlendSpace->BlendSpaceValues + AnimationBlendValueIndex;
        AnimationBlendValue->Weight = 0.f;
    }

    Assert(BlendValueFrom && BlendValueTo);

    f32 t = (BlendParameter - BlendValueFrom->Value) / (BlendValueTo->Value - BlendValueFrom->Value);

    Assert(t >= 0.f && t <= 1.f);

    BlendValueFrom->Weight = 1.f - t;
    BlendValueTo->Weight = t;
}

// TODO: FIX NORMALIZED TIME!
internal void
AnimationBlendSpacePerFrameUpdate(blend_space_1d *BlendSpace, f32 BlendParameter, f32 Delta)
{
    LinearLerpBlend(BlendSpace, BlendParameter);

    BlendSpace->NormalizedTime += Delta;

    if (BlendSpace->NormalizedTime > 1.f)
    {
        BlendSpace->NormalizedTime = 0.f;
    }

    for (u32 AnimationIndex = 0; AnimationIndex < BlendSpace->BlendSpaceValueCount; ++AnimationIndex)
    {
        blend_space_1d_value *Value = BlendSpace->BlendSpaceValues + AnimationIndex;

        if (Value->Weight > 0.f)
        {
            animation_state *AnimationState = &Value->AnimationState;

            AnimationState->Time = BlendSpace->NormalizedTime * AnimationState->Clip->Duration;

            // todo: do I need this?
            if (AnimationState->Time > AnimationState->Clip->Duration)
            {
                if (AnimationState->Clip->IsLooping)
                {
                    AnimationState->Time = 0.f;
                }
                else
                {
                    //AnimationState->Time = 0.f;
                }
            }
        }
    }
}

internal void
AnimationGraphPerFrameUpdate(animation_graph *Graph, f32 Delta, f32 BlendParameter);

internal void
AnimationNodePerFrameUpdate(animation_graph *Graph, animation_node *Node, f32 Delta, f32 BlendParameter)
{
    Assert(Node->Weight > 0.f);

    // todo: move out to separate function? (e.g. node method?)
#if 1
    animation_node *ActiveNode = GetActiveNode(Graph);

    if (Node->Params)
    {
        if (StringEquals(ActiveNode->Name, "Idle_Node#Node_0"))
        {
            // update function for Idle_Node#Node_0
            ActiveNode->Params->Time += Delta;

            if (ActiveNode->Params->Time > ActiveNode->Params->MaxTime)
            {
                ActiveNode->Params->Time = 0.f;

                if (Random01(Node->Params->RNG) > 0.5f)
                {
                    TransitionToNode(Graph, "Idle_Node#Node_1");
                }
                else
                {
                    TransitionToNode(Graph, "Idle_Node#Node_2");
                }
            }
        }
    }

    if (StringEquals(ActiveNode->Name, "Idle_Node#Node_1") || StringEquals(ActiveNode->Name, "Idle_Node#Node_2"))
    {
        // update function for Idle_Node#Node_1 and Idle_Node#Node_2
        if (ActiveNode->Animation.Time > ActiveNode->Animation.Clip->Duration)
        {
            //ActiveNode->Animation.Time = 0.f;
            TransitionToNode(Graph, "Idle_Node#Node_0");
        }
    }
#endif

    switch (Node->Type)
    {
        case AnimationNodeType_SingleMotion:
        {
            AnimationStatePerFrameUpdate(&Node->Animation, Delta);
            break;
        }
        case AnimationNodeType_BlendSpace:
        {
            AnimationBlendSpacePerFrameUpdate(Node->BlendSpace, BlendParameter, Delta);
            break;
        }
        case AnimationNodeType_Graph:
        {
            AnimationGraphPerFrameUpdate(Node->Graph, Delta, 0.f);
            break;
        }
        default:
        {
            Assert(!"Invalid animation node type");
        }
    }
}

internal void
AnimationGraphPerFrameUpdate(animation_graph *Graph, f32 Delta, f32 BlendParameter)
{
    AnimationMixerPerFrameUpdate(&Graph->Mixer, Delta);

    for (u32 NodeIndex = 0; NodeIndex < Graph->NodeCount; ++NodeIndex)
    {
        animation_node *Node = Graph->Nodes + NodeIndex;

        if (Node->Weight > 0.f)
        {
            AnimationNodePerFrameUpdate(Graph, Node, Delta, BlendParameter);
        }
    }
}

internal animation_clip *
GetAnimationClip(model *Model, const char *ClipName)
{
    animation_clip *Result = 0;

    for (u32 AnimationClipIndex = 0; AnimationClipIndex < Model->AnimationCount; ++AnimationClipIndex)
    {
        animation_clip *AnimationClip = Model->Animations + AnimationClipIndex;

        if (StringEquals(AnimationClip->Name, ClipName))
        {
            Result = AnimationClip;
            break;
        }
    }

    Assert(Result);

    return Result;
}

inline animation_state
CreateAnimationState(animation_clip *Clip)
{
    animation_state Result = {};

    Result.Clip = Clip;
    Result.Time = 0.f;

    return Result;
}

inline void
BuildAnimationNode(animation_node *Node, const char *Name, u32 Index, animation_clip *Clip)
{
    Node->Type = AnimationNodeType_SingleMotion;
    CopyString(Name, Node->Name, ArrayCount(Node->Name));
    Node->Index = Index;
    Node->Weight = 0.f;
    Node->Animation = CreateAnimationState(Clip);
    Node->Params = 0;
}

inline void
BuildAnimationNode(animation_node *Node, const char *Name, u32 Index, blend_space_1d *BlendSpace)
{
    Node->Type = AnimationNodeType_BlendSpace;
    CopyString(Name, Node->Name, ArrayCount(Node->Name));
    Node->Index = Index;
    Node->Weight = 0.f;
    Node->BlendSpace = BlendSpace;
    Node->Params = 0;
}

inline void
BuildAnimationNode(animation_node *Node, const char *Name, u32 Index, animation_graph *Graph)
{
    Node->Type = AnimationNodeType_Graph;
    CopyString(Name, Node->Name, ArrayCount(Node->Name));
    Node->Index = Index;
    Node->Weight = 0.f;
    Node->Graph = Graph;
    Node->Params = 0;
}

inline void
BuildAnimationTransition(
    animation_transition *Transition, 
    animation_node *From, 
    animation_node *To, 
    animation_transition_type Type, 
    f32 Duration = 0.f,
    animation_node *TransitionNode = 0
)
{
    Transition->From = From;
    Transition->To = To;
    Transition->Type = Type;
    Transition->Duration = Duration;
    Transition->TransitionNode = TransitionNode;
}

inline animation_node *
GetAnimationNode(animation_graph *Graph, char *NodeName)
{
    animation_node *Result = 0;

    for (u32 NodeIndex = 0; NodeIndex < Graph->NodeCount; ++NodeIndex)
    {
        animation_node *Node = Graph->Nodes + NodeIndex;

        if (StringEquals(Node->Name, NodeName))
        {
            Result = Node;
        }
    }

    Assert(Result);

    return Result;
}

inline void
ActivateAnimationNode(animation_graph *Graph, const char *NodeName)
{
    animation_node *Node= GetAnimationNode(Graph, (char *)NodeName);

    Graph->ActiveNodeIndex = Node->Index;
    EnableAnimationNode(Node);
}

internal void
BuildAnimationGraph(animation_graph *Graph, model *Model, memory_arena *Arena, random_sequence *RNG)
{
    *Graph = {};

    Graph->Mixer = {};
    Graph->NodeCount = 3;
    Graph->Nodes = PushArray(Arena, Graph->NodeCount, animation_node);

    u32 NodeIndex = 0;

    // Create Nodes

    // Idle
    animation_node *NodeIdle = Graph->Nodes + NodeIndex;

    animation_graph *IdleGraph = PushType(Arena, animation_graph);
    IdleGraph->Mixer = {};
    IdleGraph->NodeCount = 3;
    IdleGraph->Nodes = PushArray(Arena, IdleGraph->NodeCount, animation_node);
    BuildAnimationNode(NodeIdle, "Idle_Node", NodeIndex++, IdleGraph);

    {
        animation_node *IdleEntry = IdleGraph->Nodes + 0;
        BuildAnimationNode(IdleEntry, "Idle_Node#Node_0", 0, GetAnimationClip(Model, "Idle"));
        // 
        IdleEntry->Params = PushType(Arena, animation_node_params);
        IdleEntry->Params->Time = 0.f;
        //IdleEntry->Params->MaxTime = RandomBetween(RNG, 10.f, 15.f);
        IdleEntry->Params->MaxTime = 5.f;
        IdleEntry->Params->RNG = RNG;

        animation_node *LongIdleEntry = IdleGraph->Nodes + 1;
        BuildAnimationNode(LongIdleEntry, "Idle_Node#Node_1", 1, GetAnimationClip(Model, "Idle_2"));

        animation_node *LongIdle2Entry = IdleGraph->Nodes + 2;
        BuildAnimationNode(LongIdle2Entry, "Idle_Node#Node_2", 2, GetAnimationClip(Model, "Idle_3"));

        // Transitions

        // --
        IdleEntry->TransitionCount = 2;
        IdleEntry->Transitions = PushArray(Arena, IdleEntry->TransitionCount, animation_transition);

        animation_transition *IdleToLongIdleTransition = IdleEntry->Transitions + 0;
        BuildAnimationTransition(IdleToLongIdleTransition, IdleEntry, LongIdleEntry, AnimationTransitionType_Crossfade, 0.2f);

        animation_transition *IdleToLongIdle2Transition = IdleEntry->Transitions + 1;
        BuildAnimationTransition(IdleToLongIdle2Transition, IdleEntry, LongIdle2Entry, AnimationTransitionType_Crossfade, 0.2f);

        // --
        LongIdleEntry->TransitionCount = 1;
        LongIdleEntry->Transitions = PushArray(Arena, LongIdleEntry->TransitionCount, animation_transition);

        animation_transition *LongIdleToIdleTransition = LongIdleEntry->Transitions + 0;
        BuildAnimationTransition(LongIdleToIdleTransition, LongIdleEntry, IdleEntry, AnimationTransitionType_Crossfade, 0.2f);

        // --
        LongIdle2Entry->TransitionCount = 1;
        LongIdle2Entry->Transitions = PushArray(Arena, LongIdle2Entry->TransitionCount, animation_transition);

        animation_transition *LongIdle2ToIdleTransition = LongIdle2Entry->Transitions + 0;
        BuildAnimationTransition(LongIdle2ToIdleTransition, LongIdle2Entry, IdleEntry, AnimationTransitionType_Crossfade, 0.2f);

        //
        IdleGraph->Entry = IdleEntry;
        IdleGraph->ActiveNodeIndex = IdleGraph->Entry->Index;
        IdleEntry->Weight = 1.f;
    }

    // Moving
    animation_node *NodeWalking = Graph->Nodes + NodeIndex;

    blend_space_1d *BlendSpace = PushType(Arena, blend_space_1d);
    BlendSpace->BlendSpaceValueCount = 3;
    BlendSpace->BlendSpaceValues = PushArray(Arena, BlendSpace->BlendSpaceValueCount, blend_space_1d_value);

    {
        blend_space_1d_value *Value = BlendSpace->BlendSpaceValues + 0;
        Value->Value = 0.f;
        Value->AnimationState = CreateAnimationState(GetAnimationClip(Model, "Idle_4"));
    }

    {
        blend_space_1d_value *Value = BlendSpace->BlendSpaceValues + 1;
        Value->Value = 0.5f;
        Value->AnimationState = CreateAnimationState(GetAnimationClip(Model, "Walking"));
    }

    {
        blend_space_1d_value *Value = BlendSpace->BlendSpaceValues + 2;
        Value->Value = 1.f;
        Value->AnimationState = CreateAnimationState(GetAnimationClip(Model, "Running"));
    }

    BuildAnimationNode(NodeWalking, "Move_Node", NodeIndex++, BlendSpace);

    // Dancing
    animation_node *NodeDancing = Graph->Nodes + NodeIndex;
    BuildAnimationNode(NodeDancing, "Dance_Node", NodeIndex++, GetAnimationClip(Model, "Samba"));

    // Create Transitions

    // Idle Node
    NodeIdle->TransitionCount = 2;
    NodeIdle->Transitions = PushArray(Arena, NodeIdle->TransitionCount, animation_transition);

    animation_transition *IdleToWalkingTransition = NodeIdle->Transitions + 0;
    BuildAnimationTransition(IdleToWalkingTransition, NodeIdle, NodeWalking, AnimationTransitionType_Crossfade, 0.2f);

    animation_transition *IdleToDancingTransition = NodeIdle->Transitions + 1;
    BuildAnimationTransition(IdleToDancingTransition, NodeIdle, NodeDancing, AnimationTransitionType_Crossfade, 0.2f);

    // Walking Node
    NodeWalking->TransitionCount = 1;
    NodeWalking->Transitions = PushArray(Arena, NodeWalking->TransitionCount, animation_transition);

    animation_transition *WalkingToIdleTransition = NodeWalking->Transitions + 0;
    BuildAnimationTransition(WalkingToIdleTransition, NodeWalking, NodeIdle, AnimationTransitionType_Crossfade, 0.2f);

    // Dance Node
    NodeDancing->TransitionCount = 1;
    NodeDancing->Transitions = PushArray(Arena, NodeDancing->TransitionCount, animation_transition);

    animation_transition *DanceToIdleTransition = NodeDancing->Transitions + 0;
    BuildAnimationTransition(DanceToIdleTransition, NodeDancing, NodeIdle, AnimationTransitionType_Crossfade, 0.2f);

    Graph->Entry = NodeIdle;
    Graph->ActiveNodeIndex = NodeIdle->Index;
}

internal u32
GetActiveAnimationCount(animation_graph *Graph)
{
    u32 Result = 0;

    for (u32 NodeIndex = 0; NodeIndex < Graph->NodeCount; ++NodeIndex)
    {
        animation_node *Node = Graph->Nodes + NodeIndex;

        if (Node->Weight > 0.f)
        {
            switch (Node->Type)
            {
                case AnimationNodeType_SingleMotion:
                {
                    ++Result;
                    break;
                }
                case AnimationNodeType_BlendSpace:
                {
                    for (u32 Index = 0; Index < Node->BlendSpace->BlendSpaceValueCount; ++Index)
                    {
                        blend_space_1d_value *Value = Node->BlendSpace->BlendSpaceValues + Index;

                        if (Value->Weight > 0.f)
                        {
                            ++Result;
                        }
                    }

                    break;
                }
                case AnimationNodeType_Graph:
                {
                    Result += GetActiveAnimationCount(Node->Graph);
                    break;
                }
            }
        }
    }

    return Result;
}

// todo:
global u32 ActiveAnimationIndex = 0;

internal void
GetActiveAnimations(animation_graph *Graph, animation_state **ActiveAnimations, f32 GraphWeight)
{
    for (u32 NodeIndex = 0; NodeIndex < Graph->NodeCount; ++NodeIndex)
    {
        animation_node *Node = Graph->Nodes + NodeIndex;

        if (Node->Weight > 0.f)
        {
            switch (Node->Type)
            {
                case AnimationNodeType_SingleMotion:
                {
                    animation_state **ActiveAnimationState = ActiveAnimations + ActiveAnimationIndex++;
                    
                    *ActiveAnimationState = &Node->Animation;
                    (*ActiveAnimationState)->Weight = GraphWeight * Node->Weight;

                    break;
                }
                case AnimationNodeType_BlendSpace:
                {
                    for (u32 Index = 0; Index < Node->BlendSpace->BlendSpaceValueCount; ++Index)
                    {
                        blend_space_1d_value *Value = Node->BlendSpace->BlendSpaceValues + Index;

                        if (Value->Weight > 0.f)
                        {
                            animation_state **ActiveAnimationState = ActiveAnimations + ActiveAnimationIndex++;
                            *ActiveAnimationState = &Value->AnimationState;
                            (*ActiveAnimationState)->Weight = GraphWeight * Node->Weight * Value->Weight;
                        }
                    }

                    break;
                }
                case AnimationNodeType_Graph:
                {
                    GetActiveAnimations(Node->Graph, ActiveAnimations, Node->Weight);

                    break;
                }
            }
        }
    }
}

internal void
CalculateFinalSkeletonPose(animation_graph *Graph, skeleton_pose *DestPose, memory_arena *Arena)
{
    u32 ActiveAnimationCount = GetActiveAnimationCount(Graph);

    if (ActiveAnimationCount > 0)
    {
        scoped_memory ScopedMemory(Arena);
        skeleton_pose *SkeletonPoses = PushArray(ScopedMemory.Arena, ActiveAnimationCount, skeleton_pose);

        animation_state **ActiveAnimations = PushArray(ScopedMemory.Arena, ActiveAnimationCount, animation_state *);

        GetActiveAnimations(Graph, ActiveAnimations, 1.f);
        // todo:
        ActiveAnimationIndex = 0;

        f32 TotalWeight = 0.f;
        for (u32 ActiveAnimationIndex = 0; ActiveAnimationIndex < ActiveAnimationCount; ++ActiveAnimationIndex)
        {
            animation_state *Animation = ActiveAnimations[ActiveAnimationIndex];

            TotalWeight += Animation->Weight;
        }
        Assert(Abs(1.f - TotalWeight) < EPSILON);
        
#if 1
        // For debugging
        if (!(Abs(1.f - TotalWeight) < EPSILON))
        {
            GetActiveAnimationCount(Graph);
            GetActiveAnimations(Graph, ActiveAnimations, 1.f);
        }
#endif

        // Creating skeleton pose for each input clip
        for (u32 SkeletonPoseIndex = 0; SkeletonPoseIndex < ActiveAnimationCount; ++SkeletonPoseIndex)
        {
            skeleton_pose *SkeletonPose = SkeletonPoses + SkeletonPoseIndex;
            SkeletonPose->Skeleton = DestPose->Skeleton;
            SkeletonPose->LocalJointPoses = PushArray(ScopedMemory.Arena, DestPose->Skeleton->JointCount, joint_pose);

            for (u32 JointIndex = 0; JointIndex < DestPose->Skeleton->JointCount; ++JointIndex)
            {
                joint_pose *LocalJointPose = SkeletonPose->LocalJointPoses + JointIndex;
                joint_pose *SkeletonLocalJointPose = DestPose->LocalJointPoses + JointIndex;

                *LocalJointPose = *SkeletonLocalJointPose;
            }
        }

        // Extracting skeleton pose from each input clip
        // todo: multithreading?
        for (u32 AnimationIndex = 0; AnimationIndex < ActiveAnimationCount; ++AnimationIndex)
        {
            animation_state *AnimationState = ActiveAnimations[AnimationIndex];
            skeleton_pose *SkeletonPose = SkeletonPoses + AnimationIndex;

            AnimateSkeletonPose(SkeletonPose, AnimationState->Clip, AnimationState->Time);
        }

        // Lerping between all skeleton poses
        u32 CurrentPoseIndex = 0;

        skeleton_pose *Pose = SkeletonPoses + CurrentPoseIndex;
        animation_state *Animation = ActiveAnimations[CurrentPoseIndex];
        CurrentPoseIndex++;

        f32 AccumulatedWeight = Animation->Weight;

        Lerp(Pose, 0.f, Pose, DestPose);

        while (CurrentPoseIndex < ActiveAnimationCount)
        {
            skeleton_pose *NextPose = SkeletonPoses + CurrentPoseIndex;
            animation_state *NextAnimation = ActiveAnimations[CurrentPoseIndex];
            CurrentPoseIndex++;

            f32 NextWeight = NextAnimation->Weight;

            f32 t = NextWeight / (AccumulatedWeight + NextWeight);

            Lerp(Pose, t, NextPose, DestPose);

            Pose = DestPose;
            Animation = NextAnimation;
            AccumulatedWeight += NextWeight;
        }
    }
}

