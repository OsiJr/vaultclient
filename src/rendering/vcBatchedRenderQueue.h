#ifndef vcBatchedRenderQueue_h__
#define vcBatchedRenderQueue_h__

#include "udChunkedArray.h"
#include "vcPolygonModel.h"

template <typename T>
struct vcBatchedRenderQueue
{
  struct Batch
  {
    vcPolygonModel *pModel; // TODO: Make more generic

    uint32_t count;
    uint32_t capacity;
    T *pData;
  };

  udResult Init(size_t batchCount);
  udResult Deinit();

  udResult PushInstances(vcPolygonModel *pModel, const T *pData, uint32_t count);
  void Empty(); // Does not free memory. (idk about this approach)

  size_t GetBatchCount();
  const Batch* GetBatch(size_t index);

private:
  udChunkedArray<Batch> batches;
  uint32_t maxBatchSize;
  uint32_t currentBinIndex;
};

template <typename T>
inline udResult vcBatchedRenderQueue<T>::Init(size_t initialBatchCount)
{
  udResult result = udR_Success;

  UD_ERROR_CHECK(batches.Init(initialBatchCount));

  // TODO: this is graphics API specific? hardware specific?
  // on directx 10/11 compatible hardwave, the maximum constant buffer size is 65536 bytes.
  maxBatchSize = 65536 / sizeof(T);
epilogue:
  return result;
}

template <typename T>
inline udResult vcBatchedRenderQueue<T>::Deinit()
{
  for (size_t i = 0; i < batches.length; ++i)
    udFree(batches[i].pData);

  batches.Deinit();

  return udR_Success;
}

template <typename T>
inline void vcBatchedRenderQueue<T>::Empty()
{
  for (size_t i = 0; i < batches.length; ++i)
  {
    vcBatchedRenderQueue::Batch *pBatch = &batches[i];
    pBatch->count = 0;
    //pBatch->pModel = nullptr;
  }
  //batches.Clear();

  currentBinIndex = 0;
}


template <typename T>
inline udResult vcBatchedRenderQueue<T>::PushInstances(vcPolygonModel *pModel, const T *pData, uint32_t count)
{
  udResult result;

  // find an existing bin for pModel
  uint32_t sourceOffset = 0;
  for (; currentBinIndex < batches.length; ++currentBinIndex)
  {
    vcBatchedRenderQueue::Batch *pBatch = &batches[currentBinIndex];
    if (pBatch->count + count < pBatch->capacity)
    {
      memcpy(pBatch->pData + pBatch->count, pData + sourceOffset, sizeof(T) * count);
      pBatch->count += count;
      break;
    }
    else
    {
      // split it up
      uint32_t first = (pBatch->capacity - pBatch->count);
      memcpy(pBatch->pData + pBatch->count, pData + sourceOffset, sizeof(T) * first);
      pBatch->count += first;
      sourceOffset += first;
      count -= first;
    }
  }

  if (currentBinIndex == batches.length)
  {
    // Make a new queue
    vcBatchedRenderQueue::Batch *pBatch = nullptr;
    batches.PushBack(&pBatch);

    pBatch->pData = udAllocType(T, maxBatchSize, udAF_None);
    UD_ERROR_NULL(pBatch->pData, udR_MemoryAllocationFailure);

    pBatch->pModel = pModel;
    pBatch->capacity = maxBatchSize;

    memcpy(pBatch->pData, pData + sourceOffset, sizeof(T) * count);
    pBatch->count = count;
  }

  result = udR_Success;
epilogue:

  return result;
}

template <typename T>
inline size_t vcBatchedRenderQueue<T>::GetBatchCount()
{
  return batches.length;
}

template <typename T>
inline const typename vcBatchedRenderQueue<T>::Batch* vcBatchedRenderQueue<T>::GetBatch(size_t index)
{
  vcBatchedRenderQueue<T>::Batch* pBatch = &batches[index];
  return pBatch;
}

#endif//vcBatchedRenderQueue
