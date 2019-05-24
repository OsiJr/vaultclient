#include "vcSceneLayerConvert.h"
#include "vcSceneLayer_Internal.h"

#include "udChunkedArray.h"
#include "udPlatformUtil.h"

#include "vdkConvertCustom.h"
#include "vdkTriangleVoxelizer.h"

struct vcSceneLayerConvert
{
  vcSceneLayer *pSceneLayer;

  size_t leafIndex;
  size_t geometryIndex;
  size_t primIndex;

  size_t totalPrimIndex;
  size_t totalPrimCount;
  size_t totalPoints;
  vdkTriangleVoxelizer *pTrivox;

  vdkConvertCustomItemFlags flags;
  size_t pointsReturned;
  int64_t triangleArea;
  int64_t triangleAreaReturned; // Area of triangles returned so far (used to estimate how many to go)
  size_t everyNth;
  size_t everyNthAccum; // Accumulator for everyNth mode so calculations span triangles nicely

  udChunkedArray<vcSceneLayerNode*> leafNodes;
};

udResult vcSceneLayerConvert_Create(vcSceneLayerConvert **ppSceneLayerConvert, const char *pSceneLayerURL)
{
  udResult result;
  vcSceneLayerConvert *pSceneLayerConvert = nullptr;

  UD_ERROR_NULL(ppSceneLayerConvert, udR_InvalidParameter_);
  UD_ERROR_NULL(pSceneLayerURL, udR_InvalidParameter_);

  pSceneLayerConvert = udAllocType(vcSceneLayerConvert, 1, udAF_Zero);
  UD_ERROR_NULL(pSceneLayerConvert, udR_MemoryAllocationFailure);

  // Convert doesn't use a worker pool at the moment
  UD_ERROR_CHECK(vcSceneLayer_Create(&pSceneLayerConvert->pSceneLayer, nullptr, pSceneLayerURL));
  UD_ERROR_CHECK(pSceneLayerConvert->leafNodes.Init(128));

  *ppSceneLayerConvert = pSceneLayerConvert;
  result = udR_Success;

epilogue:
  return result;
}

udResult vcSceneLayerConvert_Destroy(vcSceneLayerConvert **ppSceneLayerConvert)
{
  udResult result;
  vcSceneLayerConvert *pSceneLayerConvert = nullptr;

  UD_ERROR_NULL(ppSceneLayerConvert, udR_InvalidParameter_);
  UD_ERROR_NULL(*ppSceneLayerConvert, udR_InvalidParameter_);

  pSceneLayerConvert = (*ppSceneLayerConvert);
  *ppSceneLayerConvert = nullptr;

  UD_ERROR_CHECK(vcSceneLayer_Destroy(&pSceneLayerConvert->pSceneLayer));
  vdkTriangleVoxelizer_Destroy(&pSceneLayerConvert->pTrivox);
  UD_ERROR_CHECK(pSceneLayerConvert->leafNodes.Deinit());

  udFree(pSceneLayerConvert);

  result = udR_Success;
epilogue:
  return result;
}

void vcSceneLayerConvert_RecursiveGenerateLeafNodeList(vcSceneLayerNode *pNode, udChunkedArray<vcSceneLayerNode*> &leafNodes)
{
  if (pNode->childrenCount == 0)
  {
    leafNodes.PushBack(pNode);
    return;
  }

  for (size_t i = 0; i < pNode->childrenCount; ++i)
    vcSceneLayerConvert_RecursiveGenerateLeafNodeList(&pNode->pChildren[i], leafNodes);
}

void vcSceneLayerConvert_GenerateLeafNodeList(vcSceneLayerConvert *pSceneLayerConvert)
{
  vcSceneLayerConvert_RecursiveGenerateLeafNodeList(&pSceneLayerConvert->pSceneLayer->root, pSceneLayerConvert->leafNodes);
}

/*
// TODO: this is broken, because I'm not sure about the sample center
uint32_t vcSceneLayerConvert_BilinearSample(uint8_t *pPixelData, const udFloat2 &sampleUV, float width, float height)
{
  // TODO: Not sure about the sample center... (the `+0.5` bit)
  udFloat2 uv = { udMod(udMod((sampleUV[0] + 0.5f / width) * width, width) + width, width),
                  udMod(udMod((sampleUV[1] + 0.5f / height) * height, height) + height, height) };
  udFloat2 whole = udFloat2::create(udFloor(uv.x), udFloor(uv.y));
  udFloat2 rem = udFloat2::create(uv.x - whole.x, uv.y - whole.y);

  udFloat2 uvBL = udFloat2::create(whole.x + 0.0f, whole.y + 0.0f);
  udFloat2 uvBR = udFloat2::create(udMin(whole.x + 1, width - 1), udMin(whole.y + 0, height - 1));
  udFloat2 uvTL = udFloat2::create(udMin(whole.x + 0, width - 1), udMin(whole.y + 1, height - 1));
  udFloat2 uvTR = udFloat2::create(udMin(whole.x + 1, width - 1), udMin(whole.y + 1, height - 1));

  uint8_t *pColourBL = &pPixelData[(int)(uvBL.x + uvBL.y * width) * 4];
  uint8_t *pColourBR = &pPixelData[(int)(uvBR.x + uvBR.y * width) * 4];
  uint8_t *pColourTL = &pPixelData[(int)(uvTL.x + uvTL.y * width) * 4];
  uint8_t *pColourTR = &pPixelData[(int)(uvTR.x + uvTR.y * width) * 4];

  uint8_t colour[3] = {};
  for (int c = 0; c < 3; ++c)
  {
    uint8_t colourT = udLerp(pColourTL[c], pColourTR[c], rem.x);
    uint8_t colourB = udLerp(pColourBL[c], pColourBR[c], rem.x);
    colour[c] = udLerp(colourT, colourB, rem.y);
  }

  return 0xff000000 | (colour[2] << 16) | (colour[1] << 8) | colour[0];
}
*/

udResult vcSceneLayerConvert_GatherEstimates(vcSceneLayerConvert *pSceneLayerConvert, double gridResolution)
{
  udResult result;

  UD_ERROR_NULL(pSceneLayerConvert, udR_InvalidParameter_);

  pSceneLayerConvert->triangleArea = 0;
  pSceneLayerConvert->totalPrimCount = 0;

  for (size_t i = 0; i < pSceneLayerConvert->leafNodes.length; ++i)
  {
    vcSceneLayerNode *pNode = pSceneLayerConvert->leafNodes[i];
    for (size_t g = 0; g < pNode->geometryDataCount; ++g)
    {
      vcSceneLayerNode::GeometryData *pGeometry = &pNode->pGeometryData[g];
      size_t geometryPrimitiveCount = pGeometry->vertCount / 3; // TOOD: (EVC-540) other primitive types
      pSceneLayerConvert->totalPrimCount += geometryPrimitiveCount;

      for (size_t v = 0; v < geometryPrimitiveCount; ++v)
      {
        // TODO: (EVC-540) ASSUMPTIONS! (assumed a specific vertex layout!)
        vcSceneLayerVertex* pVert0 = vcSceneLayer_GetVertex(pGeometry, v * 3 + 0);
        vcSceneLayerVertex* pVert1 = vcSceneLayer_GetVertex(pGeometry, v * 3 + 1);
        vcSceneLayerVertex* pVert2 = vcSceneLayer_GetVertex(pGeometry, v * 3 + 2);

        const udLong3 v0 = udLong3::create(pVert0->position / gridResolution);
        const udLong3 v1 = udLong3::create(pVert1->position / gridResolution);
        const udLong3 v2 = udLong3::create(pVert2->position / gridResolution);
        pSceneLayerConvert->triangleArea += (int64_t)udRound(udMag(udFloat3::create(udCross((v1 - v0), (v2 - v0)))) * 0.5);
      }
    }
  }

  result = udR_Success;

epilogue:
  return result;
}

vdkError vcSceneLayerConvert_Open(vdkConvertCustomItem *pConvertInput, uint32_t everyNth, const double origin[3], double pointResolution, vdkConvertCustomItemFlags flags)
{
  udUnused(origin);

  vcSceneLayerConvert *pSceneLayerConvert = (vcSceneLayerConvert*)pConvertInput->pData;
  vdkError result;

  pConvertInput->sourceResolution = pointResolution;
  pSceneLayerConvert->flags = flags;
  pSceneLayerConvert->everyNth = everyNth;
  pSceneLayerConvert->totalPoints = pConvertInput->pointCount;
  pSceneLayerConvert->totalPrimCount = 0;
  pSceneLayerConvert->totalPrimIndex = 0;

  if (vcSceneLayer_LoadNode(pSceneLayerConvert->pSceneLayer, nullptr, vcSLLT_Convert) != udR_Success)
    UD_ERROR_SET(vE_Failure);

  vcSceneLayerConvert_GenerateLeafNodeList(pSceneLayerConvert);
  if (vcSceneLayerConvert_GatherEstimates(pSceneLayerConvert, pConvertInput->sourceResolution) != udR_Success)
    UD_ERROR_SET(vE_Failure);

  UD_ERROR_CHECK(vdkTriangleVoxelizer_Create(&pSceneLayerConvert->pTrivox, pointResolution));

  result = vE_Success;
epilogue:

  return result;
}

void vcSceneLayerConvert_Close(vdkConvertCustomItem *pConvertInput)
{
  if (pConvertInput->pData != nullptr)
  {
    vcSceneLayerConvert *pSceneLayerConvert = (vcSceneLayerConvert*)pConvertInput->pData;
    vcSceneLayerConvert_Destroy(&pSceneLayerConvert);
  }
}

vdkError vcSceneLayerConvert_ReadPointsInt(vdkConvertCustomItem *pConvertInput, vdkConvertPointBufferInt64 *pBuffer)
{
  vdkError result;
  vcSceneLayerConvert *pSceneLayerConvert = nullptr;
  size_t pointCount = 0;
  udDouble3 sceneLayerOrigin = udDouble3::zero();
  bool allPointsReturned = false;

  UD_ERROR_NULL(pConvertInput, vE_Failure);
  UD_ERROR_NULL(pConvertInput->pData, vE_Failure);
  UD_ERROR_NULL(pBuffer, vE_Failure);
  UD_ERROR_NULL(pBuffer->pAttributes, vE_Failure);

  pSceneLayerConvert = (vcSceneLayerConvert*)pConvertInput->pData;
  memset(pBuffer->pAttributes, 0, pBuffer->attributeSize * pBuffer->pointsAllocated);
  pBuffer->pointCount = 0;
  sceneLayerOrigin = pSceneLayerConvert->pSceneLayer->root.minimumBoundingSphere.position - udDouble3::create(pSceneLayerConvert->pSceneLayer->root.minimumBoundingSphere.radius);

  // For each node
  while (pBuffer->pointCount < pBuffer->pointsAllocated && pSceneLayerConvert->leafIndex < pSceneLayerConvert->leafNodes.length)
  {
    vcSceneLayerNode *pNode = pSceneLayerConvert->leafNodes[pSceneLayerConvert->leafIndex];

    // For each geometry
    while (pBuffer->pointCount < pBuffer->pointsAllocated && pSceneLayerConvert->geometryIndex < pNode->geometryDataCount)
    {
      vcSceneLayerNode::GeometryData *pGeometry = &pNode->pGeometryData[pSceneLayerConvert->geometryIndex];
      vcSceneLayerNode::TextureData *pTextureData = nullptr;
      uint64_t geometryPrimitiveCount = pGeometry->vertCount / 3; // TOOD: (EVC-540) other primitive types

      if (pNode->textureDataCount >= 0)
        pTextureData = &pNode->pTextureData[0]; // TODO: (EVC-542) validate!

      udDouble3 geometryOriginOffset = pGeometry->originMatrix.axis.t.toVector3() - sceneLayerOrigin;

      // For each primitive
      while (pBuffer->pointCount < pBuffer->pointsAllocated && pSceneLayerConvert->primIndex < geometryPrimitiveCount)
      {
        size_t maxPoints = pBuffer->pointsAllocated - pBuffer->pointCount;

        // Get Vertices
        // TODO: (EVC-540) ASSUMPTIONS! (assumed a specific vertex layout!)
        vcSceneLayerVertex* pVert0 = vcSceneLayer_GetVertex(pGeometry, pSceneLayerConvert->primIndex * 3 + 0);
        vcSceneLayerVertex* pVert1 = vcSceneLayer_GetVertex(pGeometry, pSceneLayerConvert->primIndex * 3 + 1);
        vcSceneLayerVertex* pVert2 = vcSceneLayer_GetVertex(pGeometry, pSceneLayerConvert->primIndex * 3 + 2);

        double p0[3], p1[3], p2[3];
        for (int i = 0; i < 3; ++i)
        {
          p0[i] = pVert0->position[i] + geometryOriginOffset[i];
          p1[i] = pVert1->position[i] + geometryOriginOffset[i];
          p2[i] = pVert2->position[i] + geometryOriginOffset[i];
        }

        double *pTriPositions;
        double *pTriWeights;
        UD_ERROR_CHECK(vdkTriangleVoxelizer_SetTriangle(pSceneLayerConvert->pTrivox, p0, p1, p2));
        UD_ERROR_CHECK(vdkTriangleVoxelizer_GetPoints(pSceneLayerConvert->pTrivox, &pTriPositions, &pTriWeights, &pointCount, maxPoints));

        // Handle everyNth here in one place, slightly inefficiently but with the benefit of simplicity for the rest of the function
        if (pSceneLayerConvert->everyNth > 1)
        {
          size_t i, pc;
          for (i = pc = 0; i < pointCount; ++i)
          {
            if (++pSceneLayerConvert->everyNthAccum >= pSceneLayerConvert->everyNth)
            {
              memcpy(&pTriPositions[pc * 3], &pTriPositions[i * 3], sizeof(double) * 3);
              memcpy(&pTriWeights[pc * 3], &pTriWeights[i * 3], sizeof(double) * 3);
              ++pc;
              pSceneLayerConvert->everyNthAccum -= pSceneLayerConvert->everyNth;
            }
          }
          pointCount = pc;
        }

        // write all points from current triangle
        for (size_t i = 0; i < pointCount; ++i)
        {
          for (size_t j = 0; j < 3; ++j)
            pBuffer->pPositions[pBuffer->pointCount + i][j] = (int64_t)(pTriPositions[3 * i + j] / pConvertInput->sourceResolution);
        }

        // Assign attributes
        if (pBuffer->attributes.content & vdkCAC_ARGB)
        {
          // TODO: (EVC-540) other handle variant attribute layouts
          // Actually use `udAttribute_GetAttributeOffset(vdkCAC_ARGB, pBuffer->content)` correctly
          uint32_t *pColour = (uint32_t*)udAddBytes(pBuffer->pAttributes, pBuffer->pointCount * pBuffer->attributeSize);

          if (pTextureData != nullptr)
          {
            for (size_t i = 0; i < pointCount; ++i, pColour = udAddBytes(pColour, pBuffer->attributeSize))
            {
              udFloat2 pointUV = { 0, 0 };
              pointUV += pVert0->uv0 * pTriWeights[3 * i + 0];
              pointUV += pVert1->uv0 * pTriWeights[3 * i + 1];
              pointUV += pVert2->uv0 * pTriWeights[3 * i + 2];

              float width = (float)pTextureData->width;
              float height = (float)pTextureData->height;
              int u = (int)udMod(udMod(udRound(pointUV.x * width), width) + width, width);
              int v = (int)udMod(udMod(udRound(pointUV.y * height), height) + height, height);
              uint32_t colour = *(uint32_t*)(&pTextureData->pData[(u + v * pTextureData->width) * 4]);
              *pColour = 0xff000000 | ((colour & 0xff) << 16) | (colour & 0xff00) | ((colour & 0xff0000) >> 16);
            }
          }
          else
          {
            for (size_t i = 0; i < pointCount; ++i, pColour = udAddBytes(pColour, pBuffer->attributeSize))
              *pColour = 0xffffffff;
          }
        }

        // if current prim is done, go to next
        if (pointCount < maxPoints)
        {
          // If using triangle area estimate method, calculate area returned as we go
          if (pSceneLayerConvert->triangleArea)
          {
            const udLong3 v0 = udLong3::create(pVert0->position / pConvertInput->sourceResolution);
            const udLong3 v1 = udLong3::create(pVert1->position / pConvertInput->sourceResolution);
            const udLong3 v2 = udLong3::create(pVert2->position / pConvertInput->sourceResolution);
            pSceneLayerConvert->triangleAreaReturned += (int64_t)udRound(udMag(udFloat3::create(udCross((v1 - v0), (v2 - v0)))) * 0.5);
          }

          // current primitive done, move on to next
          ++pSceneLayerConvert->primIndex;
          ++pSceneLayerConvert->totalPrimIndex;
        }
        pBuffer->pointCount += pointCount;
      }

      if (pSceneLayerConvert->primIndex >= geometryPrimitiveCount)
      {
        // current geometry done, move onto nexet
        ++pSceneLayerConvert->geometryIndex;
        pSceneLayerConvert->primIndex = 0;
      }
    }

    if (pSceneLayerConvert->geometryIndex >= pNode->geometryDataCount)
    {
      // current leaf done, move onto next
      ++pSceneLayerConvert->leafIndex;
      pSceneLayerConvert->primIndex = 0;
      pSceneLayerConvert->geometryIndex = 0;
    }
  }

  pSceneLayerConvert->pointsReturned += pBuffer->pointCount;
  allPointsReturned = (pSceneLayerConvert->totalPrimIndex == pSceneLayerConvert->totalPrimCount);

  if (allPointsReturned)  // Always assign the actual points at the end
    pConvertInput->pointCount = pSceneLayerConvert->pointsReturned;
  else if (pSceneLayerConvert->triangleArea && pSceneLayerConvert->triangleAreaReturned) // If udOBJ_EstimatePoints was called, we can safely improve the accuracy of that estimate
    pConvertInput->pointCount = pSceneLayerConvert->triangleArea * pSceneLayerConvert->pointsReturned / pSceneLayerConvert->triangleAreaReturned;
  else if (pSceneLayerConvert->totalPrimIndex)
    pConvertInput->pointCount = pSceneLayerConvert->totalPrimCount * pSceneLayerConvert->pointsReturned / pSceneLayerConvert->totalPrimIndex;
 // pConvertInput->pointCountIsEstimate = true;

  result = vE_Success;

epilogue:
  return result;
}

vdkError vcSceneLayerConvert_AddItem(vdkContext *pContext, vdkConvertContext *pConvertContext, const char *pSceneLayerURL)
{
  vdkError result;
  vcSceneLayerConvert *pSceneLayerConvert = nullptr;
  vdkConvertCustomItem customItem = {};

  if (vcSceneLayerConvert_Create(&pSceneLayerConvert, pSceneLayerURL) != udR_Success)
    UD_ERROR_SET(vE_Failure);

  // Populate customItem
  customItem.pData = pSceneLayerConvert;
  customItem.pOpen = vcSceneLayerConvert_Open;
  customItem.pClose = vcSceneLayerConvert_Close;
  customItem.pReadPointsInt = vcSceneLayerConvert_ReadPointsInt;
  customItem.pName = pSceneLayerURL;

  customItem.content = vdkCAC_ARGB;
  customItem.srid = pSceneLayerConvert->pSceneLayer->root.zone.srid;
  customItem.sourceProjection = vdkCSP_SourceCartesian;
  customItem.pointCount = -1;
  customItem.pointCountIsEstimate = true;

  double radius = pSceneLayerConvert->pSceneLayer->root.minimumBoundingSphere.radius;
  customItem.boundMin[0] = pSceneLayerConvert->pSceneLayer->root.minimumBoundingSphere.position.x - radius;
  customItem.boundMin[1] = pSceneLayerConvert->pSceneLayer->root.minimumBoundingSphere.position.y - radius;
  customItem.boundMin[2] = pSceneLayerConvert->pSceneLayer->root.minimumBoundingSphere.position.z - radius;
  customItem.boundMax[0] = customItem.boundMin[0] + radius * 2.0;
  customItem.boundMax[1] = customItem.boundMin[1] + radius * 2.0;
  customItem.boundMax[2] = customItem.boundMin[2] + radius * 2.0;
  customItem.boundsKnown = true;

  return vdkConvert_AddCustomItem(pContext, pConvertContext, &customItem);

epilogue:
  vcSceneLayerConvert_Destroy(&pSceneLayerConvert);
  return result;
}

