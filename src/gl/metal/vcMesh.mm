#import "gl/vcMesh.h"
#import "vcMetal.h"
#import "vcViewCon.h"
#import "vcRenderer.h"
#import "udPlatformUtil.h"

uint32_t g_currIndex = 0;
uint32_t g_currVertex = 0;

udResult vcMesh_Create(vcMesh **ppMesh, const vcVertexLayoutTypes *pMeshLayout, int totalTypes, const void *pVerts, int currentVerts, const void *pIndices, int currentIndices, vcMeshFlags flags/* = vcMF_None*/)
{
  bool invalidIndexSetup = (flags & vcMF_NoIndexBuffer) || ((pIndices == nullptr && currentIndices > 0) || currentIndices == 0);
  if (ppMesh == nullptr || pMeshLayout == nullptr || totalTypes == 0 || pVerts == nullptr || currentVerts == 0)
    return udR_InvalidParameter_;

  udResult result = udR_Success;

  vcMesh *pMesh = udAllocType(vcMesh, 1, udAF_Zero);
  
  ptrdiff_t accumulatedOffset = 0;
  for (int i = 0; i < totalTypes; ++i)
  {
    switch (pMeshLayout[i])
    {
      case vcVLT_Position2:
        accumulatedOffset += 2 * sizeof(float);
        break;
      case vcVLT_Position3:
        accumulatedOffset += 3 * sizeof(float);
        break;
      case vcVLT_TextureCoords2:
        accumulatedOffset += 2 * sizeof(float);
        break;
      case vcVLT_RibbonInfo4:
        accumulatedOffset += 4 * sizeof(float);
        break;
      case vcVLT_ColourBGRA:
        accumulatedOffset += sizeof(uint32_t);
        break;
      case vcVLT_Normal3:
        accumulatedOffset += 3 * sizeof(float);
        break;
      case vcVLT_TotalTypes:
        break;
    }
  }

  pMesh->indexCount = currentIndices;
  pMesh->vertexCount = currentVerts;
  pMesh->vertexBytes = accumulatedOffset;
  
  if (!invalidIndexSetup)
  {
    if ((flags & vcMF_IndexShort))
    {
      pMesh->indexType = MTLIndexTypeUInt16;
      pMesh->indexBytes = sizeof(uint16);
    }
    else
    {
      pMesh->indexType = MTLIndexTypeUInt32;
      pMesh->indexBytes = sizeof(uint32);
    }
    udStrcpy(pMesh->iBufferIndex, 32, [NSString stringWithFormat:@"%d", g_currIndex].UTF8String);
    [_viewCon.renderer.indexBuffers setObject:[_device newBufferWithBytes:pIndices length:currentIndices * pMesh->indexBytes options:MTLStorageModeShared] forKey:[NSString stringWithUTF8String:pMesh->iBufferIndex]];
    ++g_currIndex;
  }

  udStrcpy(pMesh->vBufferIndex, 32, [NSString stringWithFormat:@"%d", g_currVertex].UTF8String);
  ++g_currVertex;
  
  @try
  {
    [_viewCon.renderer.vertBuffers setObject:[_device newBufferWithBytes:pVerts length:accumulatedOffset * currentVerts options:MTLStorageModeShared] forKey:[NSString stringWithUTF8String:pMesh->vBufferIndex]];
  }
  @catch (NSException *ne)
  {
    result = udR_Failure_;
    NSLog(@"%s", [ne.reason cStringUsingEncoding:NSUnicodeStringEncoding]);
    @throw;
  }
  
  *ppMesh = pMesh;

  return result;
}

void vcMesh_Destroy(struct vcMesh **ppMesh)
{
  if (ppMesh == nullptr || *ppMesh == nullptr)
    return;

  [_viewCon.renderer.vertBuffers removeObjectForKey:[NSString stringWithUTF8String:(*ppMesh)->vBufferIndex]];
  if ((*ppMesh)->indexCount)
    [_viewCon.renderer.indexBuffers removeObjectForKey:[NSString stringWithUTF8String:(*ppMesh)->iBufferIndex]];
  
  udFree(*ppMesh);
  *ppMesh = nullptr;
}

udResult vcMesh_UploadData(struct vcMesh *pMesh, const vcVertexLayoutTypes *pLayout, int totalTypes, const void* pVerts, int totalVerts, const void *pIndices, int totalIndices)
{
  if (pMesh == nullptr || pLayout == nullptr || totalTypes == 0 || pVerts == nullptr || totalVerts == 0)
    return udR_InvalidParameter_;

  udResult result = udR_Success;

  ptrdiff_t accumulatedOffset = 0;
  for (int i = 0; i < totalTypes; ++i)
  {
    switch (pLayout[i])
    {
      case vcVLT_Position2:
        accumulatedOffset += 2 * sizeof(float);
        break;
      case vcVLT_Position3:
        accumulatedOffset += 3 * sizeof(float);
        break;
      case vcVLT_TextureCoords2:
        accumulatedOffset += 2 * sizeof(float);
        break;
      case vcVLT_RibbonInfo4:
        accumulatedOffset += 4 * sizeof(float);
        break;
      case vcVLT_ColourBGRA:
        accumulatedOffset += sizeof(uint32_t);
        break;
      case vcVLT_Normal3:
        accumulatedOffset += 3 * sizeof(float);
        break;
      case vcVLT_TotalTypes:
        break;
    }
  }
  
  uint32_t size = accumulatedOffset * totalVerts;
  if (pMesh->vertexCount * pMesh->vertexBytes < size)
  {
    [_viewCon.renderer.vertBuffers setObject:[_device newBufferWithBytes:pVerts length:size options:MTLStorageModeShared] forKey:[NSString stringWithUTF8String:pMesh->vBufferIndex]];
  }
  else
  {
    memcpy((char *)_viewCon.renderer.vertBuffers[[NSString stringWithUTF8String:pMesh->vBufferIndex]].contents, pVerts, size);
  }
  
  pMesh->vertexCount = totalVerts;
  pMesh->vertexBytes = accumulatedOffset;
  
  if (totalIndices > 0)
  {
    if (pMesh->indexCount < (uint32_t)totalIndices)
    {
      [_viewCon.renderer.indexBuffers setObject:[_device newBufferWithBytes:pIndices length:totalIndices * pMesh->indexBytes options:MTLStorageModeShared] forKey:[NSString stringWithUTF8String:pMesh->iBufferIndex]];
    }
    else
    {
      memcpy((char *)_viewCon.renderer.indexBuffers[[NSString stringWithUTF8String:pMesh->iBufferIndex]].contents, pIndices, totalIndices * pMesh->indexBytes);
    }
  }
  pMesh->indexCount = totalIndices;
  
  return result;
}

bool vcMesh_Render(struct vcMesh *pMesh, uint32_t elementCount /* = 0*/, uint32_t startElement /* = 0*/, vcMeshRenderMode renderMode /*= vcMRM_Triangles*/)
{
  if (pMesh == nullptr || (pMesh->indexBytes > 0 && pMesh->indexCount < (elementCount + startElement) * 3) || (elementCount == 0 && startElement != 0))
    return false;

  MTLPrimitiveType primitiveType = MTLPrimitiveTypeTriangle;
  int elementsPerPrimitive;
  
  switch (renderMode)
  {
  case vcMRM_TriangleStrip:
    primitiveType = MTLPrimitiveTypeTriangleStrip;
    elementsPerPrimitive = 1;
    break;
  case vcMRM_Triangles:
    primitiveType = MTLPrimitiveTypeTriangle;
    elementsPerPrimitive = 3;
    break;
  }
  
  if (pMesh->indexCount > 0)
  {
    if (elementCount == 0)
      elementCount = pMesh->indexCount - startElement * elementsPerPrimitive;
    else
      elementCount *= elementsPerPrimitive;
    
    [_viewCon.renderer drawIndexedTriangles:_viewCon.renderer.vertBuffers[[NSString stringWithUTF8String:pMesh->vBufferIndex]] indexedBuffer:_viewCon.renderer.indexBuffers[[NSString stringWithUTF8String:pMesh->iBufferIndex]] indexCount:elementCount offset:startElement * elementsPerPrimitive * pMesh->indexBytes indexSize:pMesh->indexType primitiveType:primitiveType];
  }
  else
  {
    if (elementCount == 0)
      elementCount = pMesh->vertexCount - startElement;

    [_viewCon.renderer drawUnindexed:_viewCon.renderer.vertBuffers[[NSString stringWithUTF8String:pMesh->vBufferIndex]] vertexStart:startElement * pMesh->vertexBytes vertexCount:elementCount primitiveType:primitiveType];
  }
  
  return true;
}