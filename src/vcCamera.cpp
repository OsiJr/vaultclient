#include "vcCamera.h"

struct vcCamera
{
  udDouble3 position;
  udDouble3 yprRotation;
};

void vcCamera_Create(vcCamera **ppCamera)
{
  if (ppCamera == nullptr)
    return;

  vcCamera *pCamera = udAllocType(vcCamera, 1, udAF_None);
  pCamera->position = udDouble3::zero();
  pCamera->yprRotation = udDouble3::zero();

  *ppCamera = pCamera;
}

void vcCamera_Destroy(vcCamera **ppCamera)
{
  if(ppCamera != nullptr)
    udFree(*ppCamera);
}

udDouble4x4 vcCamera_GetMatrix(vcCamera *pCamera)
{
  udQuaternion<double> orientation = udQuaternion<double>::create(pCamera->yprRotation);
  udDouble3 lookPos = pCamera->position + orientation.apply(udDouble3::create(0, 1, 0));
  return udDouble4x4::lookAt(pCamera->position, lookPos, orientation.apply(udDouble3::create(0, 0, 1)));
}

udDoubleQuat vcCamera_GetStoredOrbitQuaternion(vcCamera *pCamera, udDouble3 orbitPosition)
{
  udDouble3 toPoint = udNormalize(orbitPosition - pCamera->position);

  udQuaternion<double> orientation = udQuaternion<double>::create(pCamera->yprRotation);
  udDouble3 forward = udNormalize(orientation.apply(udDouble3::create(0, 1, 0)));

  udDouble3 storedAxis = udNormalize3(udCross3(toPoint, forward));
  double storedAngle = udACos(udDot3(toPoint, forward));

  return udDoubleQuat::create(storedAxis, storedAngle);
}

udDouble3 vcCamera_CreateStoredRotation(vcCamera *pCamera, udDouble3 orbitPosition)
{
  udDouble3 toPoint = udNormalize3(orbitPosition - pCamera->position);

  udQuaternion<double> orientation = udQuaternion<double>::create(pCamera->yprRotation);
  udDouble3 forward = udNormalize(orientation.apply(udDouble3::create(0, 1, 0)));

  udDouble3 storedAxis = udNormalize3(udCross3(toPoint, forward));
  double storedAngle = udACos(udDot3(toPoint, forward));

  udDoubleQuat storedRotationQuat = udDoubleQuat::create(storedAxis, storedAngle);
  udDebugPrintf("--%f\n", storedAngle);
  udDouble3 deltaAngle = storedRotationQuat.eulerAngles() - orientation.eulerAngles();

  return deltaAngle;
}

void vcCamera_Orbit(vcCamera *pCamera, vcCameraSettings *pCamSettings, udDouble3 orbitPosition, udDoubleQuat storedRotation, double dx, double dy)
{
  udDouble3 toPoint = orbitPosition - pCamera->position;
  double distanceToPoint = udMag3(toPoint);
  if (distanceToPoint == 0)
    return; // cant rotate around same point with same method

  udQuaternion<double> orientation = udQuaternion<double>::create(pCamera->yprRotation);
  udDouble3 up = orientation.apply(udDouble3::create(0, 0, 1));
  udDouble3 right = orientation.apply(udDouble3::create(1, 0, 0));
  udDouble3 forward = orientation.apply(udDouble3::create(0, 1, 0));

  pCamera->position += distanceToPoint * udSin(dx) * right;
  pCamera->position += distanceToPoint * udSin(dy) * up;
  pCamera->position += distanceToPoint * (udSin(dx) * udTan(dx/2) + udSin(dy) * udTan(dy/2)) *  forward;

  if (pCamera->position - orbitPosition != udDouble3::zero() && (dx != 0 || dy != 0))
  {
    pCamera->yprRotation = udDouble4x4::lookAt(pCamera->position, orbitPosition, up).extractYPR();

    //pCamera->yprRotation += storedRotation.eulerAngles();
  }

  pCamera->yprRotation.z = 0;
}

void vcCamera_Apply(vcCamera *pCamera, vcCameraSettings *pCamSettings, udDouble3 rotationOffset, udDouble3 moveOffset, double deltaTime, float speedModifier /* = 1.f*/)
{
  float speed = pCamSettings->moveSpeed * speedModifier;

  udDouble3 addPos = udClamp(moveOffset, udDouble3::create(-1, -1, -1), udDouble3::create(1, 1, 1)); // clamp in case 2 similarly mapped movement buttons are pressed
  double vertPos = addPos.z;addPos.z = 0;
  addPos.z = 0;

  if (pCamSettings->moveMode != vcCMM_Orbit)
  {
    if (pCamSettings->invertX)
      rotationOffset.x *= -1;
    if (pCamSettings->invertY)
      rotationOffset.y *= -1;

    pCamera->yprRotation += rotationOffset;
    pCamera->yprRotation.y = udClamp(pCamera->yprRotation.y, (double)-UD_PI / 2, (double)UD_PI / 2);
  }

  addPos = (udDouble4x4::rotationYPR(pCamera->yprRotation) * udDouble4::create(addPos, 1)).toVector3();
  if (pCamSettings->moveMode == vcCMM_Helicopter)
  {
    addPos.z = 0;
    if (addPos.x != 0 || addPos.y != 0)
      addPos = udNormalize3(addPos);
  }
  addPos.z += vertPos;
  addPos *= speed * deltaTime;

  pCamera->position += addPos;
}

udDouble3 vcCamera_GetPosition(vcCamera *pCamera)
{
  return pCamera->position;
}

void vcCamera_SetPosition(vcCamera *pCamera, udDouble3 position)
{
  if(pCamera != nullptr)
    pCamera->position = position;
}

#define LENSNAME(x) #x+5

const char *lensNameArray[] =
{
  LENSNAME(vcLS_Custom),
  LENSNAME(vcLS_7mm),
  LENSNAME(vcLS_11mm),
  LENSNAME(vcLS_15mm),
  LENSNAME(vcLS_24mm),
  LENSNAME(vcLS_30mm),
  LENSNAME(vcLS_50mm),
  LENSNAME(vcLS_70mm),
  LENSNAME(vcLS_100mm),
};

UDCOMPILEASSERT(UDARRAYSIZE(lensNameArray) == vcLS_TotalLenses, "Lens Name not in Strings");


const char* const* vcCamera_GetLensNames()
{
  return lensNameArray;
}
