inline void
SetRigidBodyMass(rigid_body *Body, f32 Mass)
{
    Assert(Mass > 0.f);

    Body->InverseMass = 1.f / Mass;
}

inline void
SetInfiniteRigidBodyMass(rigid_body *Body)
{
    Body->InverseMass = 0.f;
}

inline f32
GetRigidBodyMass(rigid_body *Body)
{
    f32 Result = 1.f / Body->InverseMass;
    return Result;
}

inline b32
HasFiniteMass(rigid_body *Body)
{
    b32 Result = Body->InverseMass != 0.f;
    return Result;
}

inline aabb
GetRigidBodyAABB(rigid_body *Body)
{
    aabb Result = {};

    Result.Min = vec3(Body->Position - vec3(Body->HalfSize.x, 0.f, Body->HalfSize.z));
    Result.Max = vec3(Body->Position + vec3(Body->HalfSize.x, 2.f * Body->HalfSize.y, Body->HalfSize.z));

    return Result;
}

inline void
BuildRigidBody(rigid_body *Body, vec3 Position, quat Orientation, vec3 HalfSize, b32 RootMotionEnabled)
{
    Body->PrevPosition = Position;
    Body->Position = Position;
    Body->Orientation = Orientation;
    Body->HalfSize = HalfSize;
    Body->RootMotionEnabled = RootMotionEnabled;

    // todo: don't use at the moment? (except in Integrate)
    Body->Damping = 0.0001f;
    Body->InverseMass = 1.f / 100.f;
}

inline void
Integrate(rigid_body *Body, f32 dt)
{
    Assert(dt > 0.f);

    Body->PrevPosition = Body->Position;
    Body->Position += Body->Velocity * dt + Body->Acceleration * Square(dt) * 0.5f;
    Body->Acceleration += Body->ForceAccumulator * Body->InverseMass;
    Body->Velocity += Body->Acceleration * dt;
    Body->Velocity *= Power(Body->Damping, dt);
    Body->ForceAccumulator = vec3(0.f);
}

inline void
AddGravityForce(rigid_body *Body, vec3 Gravity)
{
    if (HasFiniteMass(Body))
    {
        Body->ForceAccumulator += Gravity * GetRigidBodyMass(Body);
    }
}

internal void
ResolveVelocity(rigid_body *Body, plane *Plane, f32 Duration, f32 Restitution)
{
    vec3 ContactNormal = Plane->Normal;
    f32 SeparatingVelocity = Dot(Body->Velocity, ContactNormal);

    if (SeparatingVelocity <= 0.f)
    {
        vec3 AccelerationCausedVelocity = Body->Acceleration;

        f32 NewSeparatingVelocity = -SeparatingVelocity * Restitution;
        f32 AccelerationCausedSeparatedVelocity = Dot(AccelerationCausedVelocity, ContactNormal) * Duration;

        if (AccelerationCausedSeparatedVelocity < 0.f)
        {
            NewSeparatingVelocity += Restitution * AccelerationCausedSeparatedVelocity;
            NewSeparatingVelocity = Max(NewSeparatingVelocity, 0.f);
        }

        f32 DeltaVelocity = NewSeparatingVelocity - SeparatingVelocity;

        if (HasFiniteMass(Body))
        {
            f32 TotalInverseMass = Body->InverseMass;
            f32 Impulse = DeltaVelocity / TotalInverseMass;

            Assert(TotalInverseMass > 0.f);

            vec3 ImpulsePerInverseMass = ContactNormal * Impulse;

            Body->Velocity += ImpulsePerInverseMass * Body->InverseMass;
        }
    }
}

internal void
ResolveVelocity(rigid_body *Body, rigid_body *OtherBody, f32 Duration, f32 Restitution)
{
    vec3 ContactNormal = Normalize(Body->Position - OtherBody->Position);
    //vec3 ContactNormal = Normalize(OtherBody->Position - Body->Position);

    vec3 RelativeVelocity = Body->Velocity - OtherBody->Velocity;
    f32 SeparatingVelocity = Dot(RelativeVelocity, ContactNormal);

    if (SeparatingVelocity <= 0.f)
    {
        vec3 AccelarationCausedVelocity = Body->Acceleration - OtherBody->Acceleration;

        f32 NewSeparatingVelocity = -SeparatingVelocity * Restitution;
        f32 AccelerationCausedSeparatedVelocity = Dot(AccelarationCausedVelocity, ContactNormal) * Duration;

        if (AccelerationCausedSeparatedVelocity < 0.f)
        {
            NewSeparatingVelocity += Restitution * AccelerationCausedSeparatedVelocity;
            NewSeparatingVelocity = Max(NewSeparatingVelocity, 0.f);
        }

        f32 DeltaVelocity = NewSeparatingVelocity - SeparatingVelocity;

        f32 TotalInverseMass = Body->InverseMass + OtherBody->InverseMass;

        if (TotalInverseMass > 0.f)
        {
            f32 Impulse = DeltaVelocity / TotalInverseMass;
            vec3 ImpulsePerInverseMass = ContactNormal * Impulse;

            Body->Velocity += ImpulsePerInverseMass * Body->InverseMass;
            OtherBody->Velocity -= ImpulsePerInverseMass * OtherBody->InverseMass;
        }
    }
}

inline void
ResolveIntepenetration(rigid_body *Body, plane *Plane, f32 Penetration)
{
    //if (Penetration > 0.f)
    {
        vec3 ContactNormal = Plane->Normal;
        f32 TotalInverseMass = Body->InverseMass;
        vec3 MovePerInverseMass = ContactNormal * (-Penetration / TotalInverseMass);
        Body->Position += MovePerInverseMass * Body->InverseMass;
    }
}

inline void
ResolveIntepenetration(rigid_body *Body, rigid_body *OtherBody, vec3 mtv, f32 Penetration)
{
    Assert(Penetration > 0.f);

    f32 TotalInverseMass = Body->InverseMass + OtherBody->InverseMass;

    //if (Penetration > 0.f)
    if (TotalInverseMass > 0.f)
    {
        //vec3 ContactNormal = Normalize(OtherBody->Position - Body->Position);
        //vec3 MovePerInverseMass = ContantNormal * (-Penetration / TotalInverseMass);
        vec3 MovePerInverseMass = mtv * (Penetration / TotalInverseMass);

        Body->Position += MovePerInverseMass * Body->InverseMass;
        OtherBody->Position -= MovePerInverseMass * OtherBody->InverseMass;
    }
}
