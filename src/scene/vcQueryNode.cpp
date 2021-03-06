#include "vcQueryNode.h"

#include "vcState.h"
#include "vcStrings.h"
#include "vcRender.h"
#include "vcInternalModels.h"
#include "vcProject.h"

#include "udMath.h"
#include "udStringUtil.h"

#include "imgui.h"
#include "imgui_ex/vcImGuiSimpleWidgets.h"

vcQueryNode::vcQueryNode(vcProject *pProject, udProjectNode *pNode, vcState *pProgramState) :
  vcSceneItem(pProject, pNode, pProgramState),
  m_shape(vcQNFS_Box),
  m_inverted(false),
  m_currentProjection(udDoubleQuat::identity()),
  m_center(udDouble3::zero()),
  m_extents(udDouble3::one()),
  m_ypr(udDouble3::zero()),
  m_pFilter(nullptr)
{
  m_loadStatus = vcSLS_Loaded;

  udQueryFilter_Create(&m_pFilter);
  udQueryFilter_SetAsBox(m_pFilter, &m_center.x, &m_extents.x, &m_ypr.x);

  OnNodeUpdate(pProgramState);
}

vcQueryNode::~vcQueryNode()
{
  udQueryFilter_Destroy(&m_pFilter);
}

void vcQueryNode::OnNodeUpdate(vcState *pProgramState)
{
  const char *pString = nullptr;

  if (udProjectNode_GetMetadataString(m_pNode, "shape", &pString, "box") == udE_Success)
  {
    if (udStrEquali(pString, "box"))
      m_shape = vcQNFS_Box;
    else if (udStrEquali(pString, "sphere"))
      m_shape = vcQNFS_Sphere;
    else if (udStrEquali(pString, "cylinder"))
      m_shape = vcQNFS_Cylinder;
  }

  udProjectNode_GetMetadataDouble(m_pNode, "size.x", &m_extents.x, 1.0);
  udProjectNode_GetMetadataDouble(m_pNode, "size.y", &m_extents.y, 1.0);
  udProjectNode_GetMetadataDouble(m_pNode, "size.z", &m_extents.z, 1.0);

  udProjectNode_GetMetadataDouble(m_pNode, "transform.rotation.y", &m_ypr.x, 0.0);
  udProjectNode_GetMetadataDouble(m_pNode, "transform.rotation.p", &m_ypr.y, 0.0);
  udProjectNode_GetMetadataDouble(m_pNode, "transform.rotation.r", &m_ypr.z, 0.0);

  vcProject_GetNodeMetadata(m_pNode, "inverted", &m_inverted, false);
  if (m_inverted) udQueryFilter_SetInverted(m_pFilter, m_inverted);

  ChangeProjection(pProgramState->geozone);

  switch (m_shape)
  {
  case vcQNFS_Box:
    udQueryFilter_SetAsBox(m_pFilter, &m_center.x, &m_extents.x, &m_ypr.x);
    break;
  case vcQNFS_Cylinder:
    udQueryFilter_SetAsCylinder(m_pFilter, &m_center.x, m_extents.x, m_extents.z, &m_ypr.x);
    break;
  case vcQNFS_Sphere:
    udQueryFilter_SetAsSphere(m_pFilter, &m_center.x, m_extents.x);
    break;
  default:
    //Something went wrong...
    break;
  }
}

void vcQueryNode::AddToScene(vcState *pProgramState, vcRenderData *pRenderData)
{
  if (!m_visible)
    return;

  if (m_selected)
  {
    vcRenderPolyInstance *pInstance = pRenderData->polyModels.PushBack();

    if (m_shape == vcQNFS_Box)
      pInstance->pModel = gInternalModels[vcInternalModelType_Cube];
    else if (m_shape == vcQNFS_Cylinder)
      pInstance->pModel = gInternalModels[vcInternalModelType_Cylinder];
    else if (m_shape == vcQNFS_Sphere)
      pInstance->pModel = gInternalModels[vcInternalModelType_Sphere];

    pInstance->worldMat = this->GetWorldSpaceMatrix();
    pInstance->pDiffuseOverride = pProgramState->pWhiteTexture;
    pInstance->cullFace = vcGLSCM_Front;
    pInstance->pSceneItem = this;
    pInstance->renderFlags = vcRenderPolyInstance::RenderFlags_Transparent;
    pInstance->tint = udFloat4::create(1.0f, 1.0f, 1.0f, 0.65f);
    pInstance->selectable = false;
  }

  if (m_pFilter != nullptr)
    pRenderData->pQueryFilter = m_pFilter;
}

void vcQueryNode::ApplyDelta(vcState *pProgramState, const udDouble4x4 &delta)
{
  udDouble4x4 matrix = delta * udDouble4x4::rotationYPR(m_ypr, m_center) * udDouble4x4::scaleNonUniform(m_extents);

  m_ypr = matrix.extractYPR();
  m_center = matrix.axis.t.toVector3();
  m_extents = udDouble3::create(udMag3(matrix.axis.x), udMag3(matrix.axis.y), udMag3(matrix.axis.z));

  // Hack: a cylinder only scale on the x axis
  if (m_shape == vcQNFS_Cylinder && delta.axis.y[1] == 1.0f)
    m_extents.x = m_extents.y;
  else if (m_shape == vcQNFS_Cylinder)// avoid jump in scale when swapping between the X & Y axis
    m_extents.y = m_extents.x;
  
  vcProject_UpdateNodeGeometryFromCartesian(m_pProject, m_pNode, pProgramState->geozone, udPGT_Point, &m_center, 1);

  udProjectNode_SetMetadataDouble(m_pNode, "size.x", m_extents.x);
  udProjectNode_SetMetadataDouble(m_pNode, "size.y", m_extents.y);
  udProjectNode_SetMetadataDouble(m_pNode, "size.z", m_extents.z);

  udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.y", m_ypr.x);
  udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.p", m_ypr.y);
  udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.r", m_ypr.z);
}

void vcQueryNode::HandleSceneExplorerUI(vcState *pProgramState, size_t *pItemID)
{
  bool changed = false;

  ImGui::InputScalarN(udTempStr("%s##FilterPosition%zu", vcString::Get("sceneFilterPosition"), *pItemID), ImGuiDataType_Double, &m_center.x, 3);
  changed |= ImGui::IsItemDeactivatedAfterEdit();

  if (m_shape != vcQNFS_Sphere)
  {
    vcIGSW_DegreesScalar(udTempStr("%s##FilterRotation%zu", vcString::Get("sceneFilterRotation"), *pItemID), &m_ypr);
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::InputScalarN(udTempStr("%s##FilterExtents%zu", vcString::Get("sceneFilterExtents"), *pItemID), ImGuiDataType_Double, &m_extents.x, 3);
    changed |= ImGui::IsItemDeactivatedAfterEdit();
  }
  else // Is a sphere
  {
    ImGui::InputScalarN(udTempStr("%s##FilterExtents%zu", vcString::Get("sceneFilterExtents"), *pItemID), ImGuiDataType_Double, &m_extents.x, 1);
    changed |= ImGui::IsItemDeactivatedAfterEdit();
  }

  if (ImGui::Checkbox(udTempStr("%s##FilterInverted%zu", vcString::Get("sceneFilterInverted"), *pItemID), &m_inverted))
  {
    changed = true;
    udQueryFilter_SetInverted(m_pFilter, m_inverted);
    udProjectNode_SetMetadataBool(m_pNode, "inverted", m_inverted);
  }

  if (changed)
  {
    static udDouble3 minDouble = udDouble3::create(1e-7, 1e-7, 1e-7);
    static udDouble3 maxDouble = udDouble3::create(1e7, 1e7, 1e7);
    m_extents = udClamp(m_extents, minDouble, maxDouble);

    udProjectNode_SetMetadataDouble(m_pNode, "size.x", m_extents.x);
    udProjectNode_SetMetadataDouble(m_pNode, "size.y", m_extents.y);
    udProjectNode_SetMetadataDouble(m_pNode, "size.z", m_extents.z);

    udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.y", m_ypr.x);
    udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.p", m_ypr.y);
    udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.r", m_ypr.z);

    this->ApplyDelta(pProgramState, udDouble4x4::identity());
  }
}

void vcQueryNode::HandleSceneEmbeddedUI(vcState * /*pProgramState*/)
{
  ImGui::Text("%s: %.6f, %.6f, %.6f", vcString::Get("sceneFilterPosition"), m_center.x, m_center.y, m_center.z);
  ImGui::Text("%s: %.6f, %.6f, %.6f", vcString::Get("sceneFilterExtents"), m_extents.x, m_extents.y, m_extents.z);

  if (ImGui::Checkbox(udTempStr("%s##EmbeddedUI", vcString::Get("sceneFilterInverted")), &m_inverted))
  {
    udQueryFilter_SetInverted(m_pFilter, m_inverted);
    udProjectNode_SetMetadataBool(m_pNode, "inverted", m_inverted);
  }
}

vcMenuBarButtonIcon vcQueryNode::GetSceneExplorerIcon()
{
  switch (m_shape)
  {
  case vcQNFS_Box:
    return vcMBBI_AddBoxFilter;
  case vcQNFS_Cylinder:
    return vcMBBI_AddCylinderFilter;
  case vcQNFS_Sphere:
    return vcMBBI_AddSphereFilter;
  case vcQNFS_Count: // Fallthrough
  case vcQNFS_None:
    return vcMBBI_AddBoxFilter;
  }

  return vcMBBI_AddBoxFilter;
}

void vcQueryNode::ChangeProjection(const udGeoZone &newZone)
{
  udDouble3 *pPoint = nullptr;
  int numPoints = 0;

  vcProject_FetchNodeGeometryAsCartesian(m_pProject, m_pNode, newZone, &pPoint, &numPoints);
  if (numPoints == 1)
    m_center = pPoint[0];

  udFree(pPoint);

  udDoubleQuat qNewProjection = vcGIS_GetQuaternion(newZone, m_center);
  udDoubleQuat qScene = udDoubleQuat::create(m_ypr);
  m_ypr = (qNewProjection * (m_currentProjection.inverse() * qScene)).eulerAngles();
  m_currentProjection = qNewProjection;

  udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.y", m_ypr.x);
  udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.p", m_ypr.y);
  udProjectNode_SetMetadataDouble(m_pNode, "transform.rotation.r", m_ypr.z);

  switch (m_shape)
  {
  case vcQNFS_Box:
    udQueryFilter_SetAsBox(m_pFilter, &m_center.x, &m_extents.x, &m_ypr.x);
    break;
  case vcQNFS_Cylinder:
    udQueryFilter_SetAsCylinder(m_pFilter, &m_center.x, m_extents.x, m_extents.z, &m_ypr.x);
    break;
  case vcQNFS_Sphere:
    udQueryFilter_SetAsSphere(m_pFilter, &m_center.x, m_extents.x);
    break;
  default:
    //Something went wrong...
    break;
  }
}

void vcQueryNode::Cleanup(vcState * /*pProgramState*/)
{
  // Nothing to clean up
}

udDouble4x4 vcQueryNode::GetWorldSpaceMatrix()
{
  if (m_shape == vcQNFS_Sphere)
    return udDouble4x4::rotationYPR(m_ypr, m_center) * udDouble4x4::scaleUniform(m_extents.x);
  else if (m_shape == vcQNFS_Cylinder)
    return udDouble4x4::rotationYPR(m_ypr, m_center) * udDouble4x4::scaleNonUniform(udDouble3::create(m_extents.x, m_extents.x, m_extents.z));

  return udDouble4x4::rotationYPR(m_ypr, m_center) * udDouble4x4::scaleNonUniform(m_extents);
}

vcGizmoAllowedControls vcQueryNode::GetAllowedControls()
{
  if (m_shape == vcQNFS_Sphere)
    return (vcGizmoAllowedControls)(vcGAC_ScaleUniform | vcGAC_Translation | vcGAC_Rotation);
  else
    return vcGAC_All;
}

void vcQueryNode::EndQuery(vcState *pProgramState, const udDouble3 & /*position*/, bool isPreview /*= false*/)
{
  udDouble3 up = vcGIS_GetWorldLocalUp(pProgramState->geozone, m_center);
  udDouble3 north = vcGIS_GetWorldLocalNorth(pProgramState->geozone, m_center);
  udPlane<double> plane = udPlane<double>::create(m_center, up);
  
  udDouble3 endPoint = {};
  if (plane.intersects(pProgramState->pActiveViewport->camera.worldMouseRay, &endPoint, nullptr))
  {
    udDouble3 horizDist = endPoint - m_center; // On horiz plane so vert is always 0

    udDouble3 pickNorth = north * horizDist;
    udDouble3 pickEast = horizDist - pickNorth;

    udDouble3 size = udDouble3::zero();
    if (m_shape == vcQNFS_Box)
    {
      size.x = udMag3(pickNorth);
      size.y = udMag3(pickEast);
      size.z = udMax(size.x, size.y);
    }
    else // vcQNFS_Cylinder & vcQNFS_Sphere
    {
      size.x = udMag3(horizDist);
      size.y = udMag3(horizDist);
      size.z = udMag3(horizDist);
    }

    vcProject_UpdateNodeGeometryFromCartesian(&pProgramState->activeProject, m_pNode, pProgramState->geozone, udPGT_Point, &m_center, 1);
    udProjectNode_SetMetadataDouble(m_pNode, "size.x", size.x);
    udProjectNode_SetMetadataDouble(m_pNode, "size.y", size.y);
    udProjectNode_SetMetadataDouble(m_pNode, "size.z", size.z);
  }

  if (!isPreview)
    pProgramState->activeTool = vcActiveTool::vcActiveTool_Select;
}
