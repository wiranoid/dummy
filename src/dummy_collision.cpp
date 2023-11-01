#include "dummy.h"

// https://stackoverflow.com/questions/47866571/simple-oriented-bounding-box-obb-collision-detection-explaining

inline aabb
GetColliderBounds(collider *Collider)
{
    aabb Result = {};

    switch (Collider->Type)
    {
        case Collider_Box:
        {
            box_collider Box = Collider->BoxCollider;
            vec3 HalfSize = Box.Size / 2;

            Result.Min = Box.Center - HalfSize;
            Result.Max = Box.Center + HalfSize;

            break;
        }
        case Collider_Sphere:
        {
            sphere_collider Sphere = Collider->SphereCollider;

            Result.Min = Sphere.Center - vec3(Sphere.Radius);
            Result.Max = Sphere.Center + vec3(Sphere.Radius);

            break;
        }
    }

    return Result;
}

inline void
UpdateColliderPosition(collider *Collider, vec3 BasePosition)
{
    switch (Collider->Type)
    {
        case Collider_Box:
        {
            box_collider *BoxCollider = &Collider->BoxCollider;
            BoxCollider->Center = BasePosition + vec3(0.f, BoxCollider->Size.y / 2.f, 0.f);

            break;
        }
        case Collider_Sphere:
        {
            sphere_collider *SphereCollider = &Collider->SphereCollider;
            SphereCollider->Center = BasePosition + vec3(0.f, SphereCollider->Radius, 0.f);

            break;
        }
    }
}

dummy_internal aabb
GetEntityBounds(game_entity *Entity)
{
    aabb Result = {};

    if (Entity->Collider)
    {
        Result = GetColliderBounds(Entity->Collider);
    }
    else if (Entity->Model)
    {
        Result = UpdateBounds(Entity->Model->Bounds, Entity->Transform);
    }
    else if (Entity->PointLight || Entity->ParticleEmitter || Entity->AudioSource)
    {
        aabb Bounds =
        {
            .Min = vec3(-0.1f),
            .Max = vec3(0.1f)
        };

        Result = UpdateBounds(Bounds, Entity->Transform);
    }
    else
    {
        aabb Bounds =
        {
            .Min = vec3(-0.5f, 0.f, -0.5f),
            .Max = vec3(0.5f, 1.f, 0.5f)
        };

        Result = UpdateBounds(Bounds, Entity->Transform);
    }

    return Result;
}

dummy_internal bool32
TestAxis(vec3 Axis, f32 MinA, f32 MaxA, f32 MinB, f32 MaxB, vec3 *mtvAxis, f32 *mtvDistance)
{
    // [Separating Axis Theorem]
    // � Two convex shapes only overlap if they overlap on all axes of separation
    // � In order to create accurate responses we need to find the collision vector (Minimum Translation Vector)
    // � Find if the two boxes intersect along a single axis
    // � Compute the intersection interval for that axis
    // � Keep the smallest intersection/penetration value

    f32 AxisLengthSquared = Dot(Axis, Axis);

    // If the axis is degenerate then ignore
    if (AxisLengthSquared < EPSILON)
    {
        return true;
    }

    // Calculate the two possible overlap ranges
    // Either we overlap on the left or the right sides
    f32 d0 = (MaxB - MinA);   // 'Left' side
    f32 d1 = (MaxA - MinB);   // 'Right' side

    // Intervals do not overlap, so no intersection
    if (d0 <= 0.0f || d1 <= 0.0f)
    {
        return false;
    }

    // Find out if we overlap on the 'right' or 'left' of the object.
    f32 Overlap = (d0 < d1) ? d0 : -d1;

    // The mtd vector for that axis
    vec3 Sep = Axis * (Overlap / AxisLengthSquared);

    // The mtd vector length squared
    f32 SepLengthSquared = Dot(Sep, Sep);

    // If that vector is smaller than our computed Minimum Translation Distance use that vector as our current MTV distance
    if (SepLengthSquared < *mtvDistance)
    {
        *mtvDistance = SepLengthSquared;
        *mtvAxis = Sep;
    }

    return true;
}

dummy_internal bool32
TestAABBAABB(aabb a, aabb b, vec3 *mtv)
{
    // [Minimum Translation Vector]
    f32 mtvDistance = F32_MAX;              // Set current minimum distance (max float value so next value is always less)
    vec3 mtvAxis;                           // Axis along which to travel with the minimum distance

    // [Axes of potential separation]
    vec3 xAxis = vec3(1.f, 0.f, 0.f);
    vec3 yAxis = vec3(0.f, 1.f, 0.f);
    vec3 zAxis = vec3(0.f, 0.f, 1.f);

    // [X Axis]
    if (!TestAxis(xAxis, a.Min.x, a.Max.x, b.Min.x, b.Max.x, &mtvAxis, &mtvDistance))
    {
        return false;
    }

    // [Y Axis]
    if (!TestAxis(yAxis, a.Min.y, a.Max.y, b.Min.y, b.Max.y, &mtvAxis, &mtvDistance))
    {
        return false;
    }

    // [Z Axis]
    if (!TestAxis(zAxis, a.Min.z, a.Max.z, b.Min.z, b.Max.z, &mtvAxis, &mtvDistance))
    {
        return false;
    }

    // Multiply the penetration depth by itself plus a small increment
    // When the penetration is resolved using the MTV, it will no longer intersect
    f32 Penetration = Sqrt(mtvDistance) * 1.001f;

    // Calculate Minimum Translation Vector (MTV) [normal * penetration]
    *mtv = Normalize(mtvAxis) * Penetration;

    return true;
}

inline bool32
TestAABBAABB(aabb a, aabb b)
{
    // Exit with no intersection if separated along an axis
    if (a.Max.x < b.Min.x || a.Min.x > b.Max.x) return false;
    if (a.Max.y < b.Min.y || a.Min.y > b.Max.y) return false;
    if (a.Max.z < b.Min.z || a.Min.z > b.Max.z) return false;

    // Overlapping on all axes means AABBs are intersecting
    return true;
}

dummy_internal bool32
TestAABBPlane(aabb Box, plane Plane)
{
    vec3 BoxCenter = (Box.Min + Box.Max) * 0.5f;
    vec3 BoxExtents = Box.Max - BoxCenter;

    // Compute the projection interval radius of AABB onto L(t) = BoxCenter + t * Plane.Normal
    f32 Radius = Dot(BoxExtents, Abs(Plane.Normal));
    // Compute distance of AABB center from plane
    f32 Distance = Dot(Plane.Normal, BoxCenter) - Plane.Distance;
    
    // Intersection occurs when distance falls within [-Radius, +Radius] interval
    bool32 Result = Abs(Distance) <= Radius;
    
    return Result;
}

dummy_internal bool32
IntersectMovingAABBAABB(aabb a, aabb b, vec3 VelocityA, vec3 VelocityB, f32 *tFirst, f32 *tLast)
{
    // Exit early if a and b initially overlapping
    if (TestAABBAABB(a, b))
    {
        *tFirst = 0.f;
        *tLast = 0.f;
        return true;
    }

    // Use relative velocity; effectively treating a as stationary
    vec3 RelativeVelocity = VelocityB - VelocityA;

    // Initialize times of first and last contact
    *tFirst = 0.f;
    *tLast = 1.f;

    // For each axis, determine times of first and last contact, if any
    for (u32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        if (RelativeVelocity[AxisIndex] < 0.f)
        {
            if (b.Max[AxisIndex] < a.Min[AxisIndex])
            {
                // Nonintersecting and moving apart
                return false;
            }
            
            if (a.Max[AxisIndex] < b.Min[AxisIndex])
            {
                *tFirst = Max((a.Max[AxisIndex] - b.Min[AxisIndex]) / RelativeVelocity[AxisIndex], *tFirst);
            }

            if (b.Max[AxisIndex] > a.Min[AxisIndex])
            {
                *tLast = Min((a.Min[AxisIndex] - b.Max[AxisIndex]) / RelativeVelocity[AxisIndex], *tLast);
            }
        }

        if (RelativeVelocity[AxisIndex] > 0.f)
        {
            if (b.Min[AxisIndex] > a.Max[AxisIndex])
            {
                // Nonintersecting and moving apart
                return false;
            }

            if (b.Max[AxisIndex] < a.Min[AxisIndex])
            {
                *tFirst = Max((a.Min[AxisIndex] - b.Max[AxisIndex]) / RelativeVelocity[AxisIndex], *tFirst);
            }

            if (a.Max[AxisIndex] > b.Min[AxisIndex])
            {
                *tLast = Min((a.Max[AxisIndex] - b.Min[AxisIndex]) / RelativeVelocity[AxisIndex], *tLast);
            }
        }

        // No overlap possible if time of first contact occurs after time of last contact
        if (*tFirst > *tLast)
        {
            return false;
        }
    }

    return true;
}

/*
    Fast Ray-Box Intersection
    by Andrew Woo
    from "Graphics Gems", Academic Press, 1990
*/
bool32 IntersectRayAABB(ray Ray, aabb Box, vec3 &Coord)
{
    vec3 RayOrigin = Ray.Origin;
    vec3 RayDirection = Ray.Direction;
    vec3 BoxMin = Box.Min;
    vec3 BoxMax = Box.Max;

    i32 Right = 0;
    i32 Left = 1;
    i32 Middle = 2;

    bool32 IsInside = true;
    i32 WhichPlane;
    f32 MaxT[3];
    f32 CandidatePlanes[3];
    u8 Quadrant[3];

    /* Find candidate planes; this loop can be avoided if
       rays cast all from the eye(assume perpsective view) */
    for (u32 i = 0; i < 3; i++)
    {
        if (RayOrigin[i] < BoxMin[i])
        {
            Quadrant[i] = Left;
            CandidatePlanes[i] = BoxMin[i];
            IsInside = false;
        }
        else if (RayOrigin[i] > BoxMax[i])
        {
            Quadrant[i] = Right;
            CandidatePlanes[i] = BoxMax[i];
            IsInside = false;
        }
        else
        {
            Quadrant[i] = Middle;
        }
    }

    /* Ray origin inside bounding box */
    if (IsInside)
    {
        Coord = RayOrigin;
        return true;
    }

    /* Calculate T distances to candidate planes */
    for (u32 i = 0; i < 3; i++)
    {
        if (Quadrant[i] != Middle && RayDirection[i] != 0.f)
        {
            MaxT[i] = (CandidatePlanes[i] - RayOrigin[i]) / RayDirection[i];
        }
        else
        {
            MaxT[i] = -1.f;
        }
    }

    /* Get largest of the MaxT's for final choice of intersection */
    WhichPlane = 0;
    for (u32 i = 1; i < 3; i++)
    {
        if (MaxT[WhichPlane] < MaxT[i])
        {
            WhichPlane = i;
        }
    }

    /* Check final candidate actually inside box */
    if (MaxT[WhichPlane] < 0.f)
    {
        return false;
    }

    for (u32 i = 0; i < 3; i++)
    {
        if (WhichPlane != i)
        {
            Coord[i] = RayOrigin[i] + MaxT[WhichPlane] * RayDirection[i];

            if (Coord[i] < BoxMin[i] || Coord[i] > BoxMax[i])
            {
                return false;
            }
        }
        else 
        {
            Coord[i] = CandidatePlanes[i];
        }
    }

    /* Ray hits box */
    return true;     
}

dummy_internal f32
GetAABBPlaneMinDistance(aabb Box, plane Plane)
{
    vec3 BoxCenter = (Box.Min + Box.Max) * 0.5f;
    vec3 BoxExtents = Box.Max - BoxCenter;

    f32 Radius = Dot(BoxExtents, Abs(Plane.Normal));
    f32 Distance = Dot(Plane.Normal, BoxCenter) - Plane.Distance;

    f32 Result = Distance - Radius;

    return Result;
}

dummy_internal bool32
TestColliders(collider *a, collider *b, vec3 *mtv)
{
    if (a->Type == Collider_Box && b->Type == Collider_Box)
    {
        return TestAABBAABB(GetColliderBounds(a), GetColliderBounds(b), mtv);
    }
    else
    {
        Assert(!"Not implemented");
    }

    return false;
}
