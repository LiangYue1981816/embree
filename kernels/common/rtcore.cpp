// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#ifdef _WIN32
#  define RTCORE_API extern "C" __declspec(dllexport)
#else
#  define RTCORE_API extern "C" __attribute__ ((visibility ("default")))
#endif

#include "default.h"
#include "device.h"
#include "scene.h"
#include "context.h"
#include "../../include/embree2/rtcore_ray.h"

namespace embree
{  
  /* mutex to make API thread safe */
  static MutexSys g_mutex;

  RTCORE_API RTCDevice rtcNewDevice(const char* cfg)
  {
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewDevice);
    Lock<MutexSys> lock(g_mutex);
    return (RTCDevice) new Device(cfg,false);
    RTCORE_CATCH_END(nullptr);
    return (RTCDevice) nullptr;
  }

  RTCORE_API void rtcDeleteDevice(RTCDevice device) 
  {
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeleteDevice);
    RTCORE_VERIFY_HANDLE(device);
    Lock<MutexSys> lock(g_mutex);
    delete (Device*) device;
    RTCORE_CATCH_END(nullptr);
  }

  RTCORE_API void rtcDeviceSetParameter1i(RTCDevice hdevice, const RTCParameter parm, ssize_t val)
  {
    Device* device = (Device*) hdevice;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeviceSetParameter1i);
    const bool internal_parm = (size_t)parm >= 1000000 && (size_t)parm < 1000004;
    if (!internal_parm) RTCORE_VERIFY_HANDLE(hdevice); // allow NULL device for special internal settings
    Lock<MutexSys> lock(g_mutex);
    device->setParameter1i(parm,val);
    RTCORE_CATCH_END(device);
  }

  RTCORE_API ssize_t rtcDeviceGetParameter1i(RTCDevice hdevice, const RTCParameter parm)
  {
    Device* device = (Device*) hdevice;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeviceGetParameter1i);
    RTCORE_VERIFY_HANDLE(hdevice);
    Lock<MutexSys> lock(g_mutex);
    return device->getParameter1i(parm);
    RTCORE_CATCH_END(device);
    return 0;
  }

  RTCORE_API RTCError rtcDeviceGetError(RTCDevice hdevice)
  {
    Device* device = (Device*) hdevice;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeviceGetError);
    if (device == nullptr) return Device::getThreadErrorCode();
    else                   return device->getDeviceErrorCode();
    RTCORE_CATCH_END(device);
    return RTC_UNKNOWN_ERROR;
  }

  RTCORE_API void rtcDeviceSetErrorFunction(RTCDevice hdevice, RTCErrorFunc func, void* userPtr) 
  {
    Device* device = (Device*) hdevice;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeviceSetErrorFunction);
    RTCORE_VERIFY_HANDLE(hdevice);
    device->setErrorFunction(func,userPtr);
    RTCORE_CATCH_END(device);
  }

  RTCORE_API void rtcDeviceSetMemoryMonitorFunction(RTCDevice hdevice, RTCMemoryMonitorFunc func, void* userPtr) 
  {
    Device* device = (Device*) hdevice;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeviceSetMemoryMonitorFunction);
    device->setMemoryMonitorFunction(func,userPtr);
    RTCORE_CATCH_END(device);
  }

  RTCORE_API void rtcDebug() 
  {
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDebug);

#if defined(EMBREE_STAT_COUNTERS)
    Stat::print(std::cout);
    Stat::clear();
#endif

    RTCORE_CATCH_END(nullptr);
  }

  RTCORE_API RTCScene rtcDeviceNewScene (RTCDevice device, RTCSceneFlags flags, RTCAlgorithmFlags aflags) 
  {
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeviceNewScene);
    RTCORE_VERIFY_HANDLE(device);
    if (!isCoherent(flags) && !isIncoherent(flags)) flags = RTCSceneFlags(flags | RTC_SCENE_INCOHERENT);
    return (RTCScene) new Scene((Device*)device,flags,aflags);
    RTCORE_CATCH_END((Device*)device);
    return nullptr;
  }

  RTCORE_API void rtcSetProgressMonitorFunction(RTCScene hscene, RTCProgressMonitorFunc func, void* ptr) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetProgressMonitorFunction);
    RTCORE_VERIFY_HANDLE(hscene);
    scene->setProgressMonitorFunction(func,ptr);
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcCommit (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcCommit);
    RTCORE_VERIFY_HANDLE(hscene);
    scene->commit(0,0,true);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcCommitJoin (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcCommitJoin);
    RTCORE_VERIFY_HANDLE(hscene);
    scene->commit(0,0,false);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcCommitThread(RTCScene hscene, unsigned int threadID, unsigned int numThreads) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcCommitThread);
    RTCORE_VERIFY_HANDLE(hscene);

    if (unlikely(numThreads == 0)) 
      throw_RTCError(RTC_INVALID_OPERATION,"invalid number of threads specified");
    if (unlikely(threadID >= numThreads)) 
      throw_RTCError(RTC_INVALID_OPERATION,"invalid thread ID");

    /* for best performance set FTZ and DAZ flags in the MXCSR control and status register */
    unsigned int mxcsr = _mm_getcsr();
    _mm_setcsr(mxcsr | /* FTZ */ (1<<15) | /* DAZ */ (1<<6));
    
    /* perform scene build */
    scene->commit(threadID,numThreads,false);

    /* reset MXCSR register again */
    _mm_setcsr(mxcsr);
    
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcGetBounds(RTCScene hscene, RTCBounds& bounds_o)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcGetBounds);
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    BBox3fa bounds = scene->bounds.bounds();
    bounds_o.lower_x = bounds.lower.x;
    bounds_o.lower_y = bounds.lower.y;
    bounds_o.lower_z = bounds.lower.z;
    bounds_o.align0  = 0;
    bounds_o.upper_x = bounds.upper.x;
    bounds_o.upper_y = bounds.upper.y;
    bounds_o.upper_z = bounds.upper.z;
    bounds_o.align1  = 0;
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcGetLinearBounds(RTCScene hscene, RTCBounds* bounds_o)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcGetBounds);
    RTCORE_VERIFY_HANDLE(hscene);
    if (bounds_o == nullptr)
      throw_RTCError(RTC_INVALID_OPERATION,"invalid destination pointer");
    if (scene->isModified())
      throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    
    bounds_o[0].lower_x = scene->bounds.bounds0.lower.x;
    bounds_o[0].lower_y = scene->bounds.bounds0.lower.y;
    bounds_o[0].lower_z = scene->bounds.bounds0.lower.z;
    bounds_o[0].align0  = 0;
    bounds_o[0].upper_x = scene->bounds.bounds0.upper.x;
    bounds_o[0].upper_y = scene->bounds.bounds0.upper.y;
    bounds_o[0].upper_z = scene->bounds.bounds0.upper.z;
    bounds_o[0].align1  = 0;
    bounds_o[1].lower_x = scene->bounds.bounds1.lower.x;
    bounds_o[1].lower_y = scene->bounds.bounds1.lower.y;
    bounds_o[1].lower_z = scene->bounds.bounds1.lower.z;
    bounds_o[1].align0  = 0;
    bounds_o[1].upper_x = scene->bounds.bounds1.upper.x;
    bounds_o[1].upper_y = scene->bounds.bounds1.upper.y;
    bounds_o[1].upper_z = scene->bounds.bounds1.upper.z;
    bounds_o[1].align1  = 0;
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcIntersect1Ex (RTCScene hscene, const RTCIntersectContext* user_context, RTCRay& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersect1Ex);
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)&ray) & 0x0F        ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    STAT3(normal.travs,1,1,1);
    IntersectContext context(scene,user_context);
    scene->intersectors.intersect(ray,&context);
#if defined(DEBUG)
    ((Ray&)ray).verifyHit();
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcIntersect4Ex (const void* valid, RTCScene hscene, const RTCIntersectContext* user_context, RTCRay4& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersect4Ex);

#if defined(EMBREE_TARGET_SIMD4) && defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x0F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)&ray ) & 0x0F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);
    IntersectContext context(scene,user_context);
    scene->intersectors.intersect4(valid,ray,&context);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcIntersect4Ex not supported");  
#endif
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcIntersect8Ex (const void* valid, RTCScene hscene, const RTCIntersectContext* user_context, RTCRay8& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersect8Ex);

#if defined(EMBREE_TARGET_SIMD8) && defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x1F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "mask not aligned to 32 bytes");   
    if (((size_t)&ray ) & 0x1F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 32 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<8; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
    scene->intersectors.intersect8(valid,ray,&context);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcIntersect8Ex not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcIntersect16Ex (const void* valid, RTCScene hscene, const RTCIntersectContext* user_context, RTCRay16& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersect16Ex);

#if defined(EMBREE_TARGET_SIMD16) && defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x3F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "mask not aligned to 64 bytes");   
    if (((size_t)&ray ) & 0x3F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 64 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<16; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
    scene->intersectors.intersect16(valid,ray,&context);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcIntersect16Ex not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcIntersect1M (RTCScene hscene, const RTCIntersectContext* user_context, RTCRay* rays, const size_t M, const size_t stride) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersect1M);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays ) & 0x03) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,M,M,M);
    IntersectContext context(scene,user_context);

    /* fast codepath for single rays */
    if (likely(M == 1)) {
      if (likely(rays->tnear <= rays->tfar)) 
        scene->intersectors.intersect(*rays,&context);
    } 

    /* codepath for streams */
    else {
      scene->device->rayStreamFilters.filterAOS(scene,rays,M,stride,&context,true);   
    }
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcIntersect1M not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcIntersect1Mp (RTCScene hscene, const RTCIntersectContext* user_context, RTCRay** rays, const size_t M) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersect1Mp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays ) & 0x03) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,M,M,M);
    IntersectContext context(scene,user_context);

    /* fast codepath for single rays */
    if (likely(M == 1)) {
      if (likely(rays[0]->tnear <= rays[0]->tfar)) 
        scene->intersectors.intersect(*rays[0],&context);
    } 

    /* codepath for streams */
    else {
      scene->device->rayStreamFilters.filterAOP(scene,rays,M,&context,true);   
    }
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcIntersect1Mp not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcIntersectNM (RTCScene hscene, const RTCIntersectContext* user_context, struct RTCRayN* rays, const size_t N, const size_t M, const size_t stride) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersectNM);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays ) & 0x03) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,N*M,N*M,N*M);
    IntersectContext context(scene,user_context);

    /* code path for single ray streams */
    if (likely(N == 1))
    {
      /* fast code path for streams of size 1 */
      if (likely(M == 1)) {
        if (likely(((RTCRay*)rays)->tnear <= ((RTCRay*)rays)->tfar))
          scene->intersectors.intersect(*(RTCRay*)rays,&context);
      } 
      /* normal codepath for single ray streams */
      else {
        scene->device->rayStreamFilters.filterAOS(scene,(RTCRay*)rays,M,stride,&context,true);
      }
    }
    /* code path for ray packet streams */
    else {
      scene->device->rayStreamFilters.filterSOA(scene,(char*)rays,N,M,stride,&context,true);
    }
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcIntersectNM not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcIntersectNp (RTCScene hscene, const RTCIntersectContext* user_context, const RTCRayNp& rays, const size_t N) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcIntersectNp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays.orgx   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.orgx not aligned to 4 bytes");   
    if (((size_t)rays.orgy   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.orgy not aligned to 4 bytes");   
    if (((size_t)rays.orgz   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.orgz not aligned to 4 bytes");   
    if (((size_t)rays.dirx   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.dirx not aligned to 4 bytes");   
    if (((size_t)rays.diry   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.diry not aligned to 4 bytes");   
    if (((size_t)rays.dirz   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.dirz not aligned to 4 bytes");   
    if (((size_t)rays.tnear  ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.dirx not aligned to 4 bytes");   
    if (((size_t)rays.tfar   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.tnear not aligned to 4 bytes");   
    if (((size_t)rays.time   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.time not aligned to 4 bytes");   
    if (((size_t)rays.mask   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.mask not aligned to 4 bytes");   
    if (((size_t)rays.Ngx    ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.Ngx not aligned to 4 bytes");   
    if (((size_t)rays.Ngy    ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.Ngy not aligned to 4 bytes");   
    if (((size_t)rays.Ngz    ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.Ngz not aligned to 4 bytes");   
    if (((size_t)rays.u      ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.u not aligned to 4 bytes");   
    if (((size_t)rays.v      ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.v not aligned to 4 bytes");   
    if (((size_t)rays.geomID ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.geomID not aligned to 4 bytes");   
    if (((size_t)rays.primID ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.primID not aligned to 4 bytes");   
    if (((size_t)rays.instID ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.instID not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,N,N,N);
    IntersectContext context(scene,user_context);
    scene->device->rayStreamFilters.filterSOP(scene,rays,N,&context,true);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcIntersectNp not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcOccluded1Ex (RTCScene hscene, const RTCIntersectContext* user_context, RTCRay& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccluded1Ex);
    STAT3(shadow.travs,1,1,1);
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)&ray) & 0x0F        ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    IntersectContext context(scene,user_context);
    scene->intersectors.occluded(ray,&context);
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcOccluded4Ex (const void* valid, RTCScene hscene, const RTCIntersectContext* user_context, RTCRay4& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccluded4Ex);

#if defined(EMBREE_TARGET_SIMD4) && defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x0F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)&ray ) & 0x0F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);
    IntersectContext context(scene,user_context);
    scene->intersectors.occluded4(valid,ray,&context);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcOccluded4Ex not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }
 
  RTCORE_API void rtcOccluded8Ex (const void* valid, RTCScene hscene, const RTCIntersectContext* user_context, RTCRay8& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccluded8Ex);

#if defined(EMBREE_TARGET_SIMD8) && defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x1F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "mask not aligned to 32 bytes");   
    if (((size_t)&ray ) & 0x1F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 32 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<8; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
    scene->intersectors.occluded8(valid,ray,&context);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcOccluded8Ex not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcOccluded16Ex (const void* valid, RTCScene hscene, const RTCIntersectContext* user_context, RTCRay16& ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccluded16Ex);

#if defined(EMBREE_TARGET_SIMD16) && defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x3F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "mask not aligned to 64 bytes");   
    if (((size_t)&ray ) & 0x3F       ) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 64 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<16; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
    scene->intersectors.occluded16(valid,ray,&context);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcOccluded16Ex not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcOccluded1M(RTCScene hscene, const RTCIntersectContext* user_context, RTCRay* rays, const size_t M, const size_t stride) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccluded1M);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays ) & 0x03) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,M,M,M);
    IntersectContext context(scene,user_context);

    /* fast codepath for streams of size 1 */
    if (likely(M == 1)) {
      if (likely(rays->tnear <= rays->tfar)) 
        scene->intersectors.occluded (*rays,&context);
    } 
    /* codepath for normal streams */
    else {
      scene->device->rayStreamFilters.filterAOS(scene,rays,M,stride,&context,false);
    }
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcOccluded1M not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcOccluded1Mp(RTCScene hscene, const RTCIntersectContext* user_context, RTCRay** rays, const size_t M) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccluded1Mp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays ) & 0x03) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,M,M,M);
    IntersectContext context(scene,user_context);

    /* fast codepath for streams of size 1 */
    if (likely(M == 1)) {
      if (likely(rays[0]->tnear <= rays[0]->tfar)) 
        scene->intersectors.occluded (*rays[0],&context);
    } 
    /* codepath for normal streams */
    else {
      scene->device->rayStreamFilters.filterAOP(scene,rays,M,&context,false);
    }
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcOccluded1Mp not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcOccludedNM(RTCScene hscene, const RTCIntersectContext* user_context, RTCRayN* rays, const size_t N, const size_t M, const size_t stride) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccludedNM);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (stride < sizeof(RTCRay)) throw_RTCError(RTC_INVALID_OPERATION,"stride too small");
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays ) & 0x03) throw_RTCError(RTC_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,N*M,N*N,N*N);
    IntersectContext context(scene,user_context);

    /* codepath for single rays */
    if (likely(N == 1))
    {
      /* fast path for streams of size 1 */
      if (likely(M == 1)) {
        if (likely(((RTCRay*)rays)->tnear <= ((RTCRay*)rays)->tfar))
          scene->intersectors.occluded (*(RTCRay*)rays,&context);
      } 
      /* codepath for normal ray streams */
      else {
        scene->device->rayStreamFilters.filterAOS(scene,(RTCRay*)rays,M,stride,&context,false);
      }
    }
    /* code path for ray packet streams */
    else {
      scene->device->rayStreamFilters.filterSOA(scene,(char*)rays,N,M,stride,&context,false);
    }
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcOccludedNM not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcOccludedNp(RTCScene hscene, const RTCIntersectContext* user_context, const RTCRayNp& rays, const size_t N) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcOccludedNp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rays.orgx   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.orgx not aligned to 4 bytes");   
    if (((size_t)rays.orgy   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.orgy not aligned to 4 bytes");   
    if (((size_t)rays.orgz   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.orgz not aligned to 4 bytes");   
    if (((size_t)rays.dirx   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.dirx not aligned to 4 bytes");   
    if (((size_t)rays.diry   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.diry not aligned to 4 bytes");   
    if (((size_t)rays.dirz   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.dirz not aligned to 4 bytes");   
    if (((size_t)rays.tnear  ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.dirx not aligned to 4 bytes");   
    if (((size_t)rays.tfar   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.tnear not aligned to 4 bytes");   
    if (((size_t)rays.time   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.time not aligned to 4 bytes");   
    if (((size_t)rays.mask   ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.mask not aligned to 4 bytes");   
    if (((size_t)rays.Ngx    ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.Ngx not aligned to 4 bytes");   
    if (((size_t)rays.Ngy    ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.Ngy not aligned to 4 bytes");   
    if (((size_t)rays.Ngz    ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.Ngz not aligned to 4 bytes");   
    if (((size_t)rays.u      ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.u not aligned to 4 bytes");   
    if (((size_t)rays.v      ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.v not aligned to 4 bytes");   
    if (((size_t)rays.geomID ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.geomID not aligned to 4 bytes");   
    if (((size_t)rays.primID ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.primID not aligned to 4 bytes");   
    if (((size_t)rays.instID ) & 0x03 ) throw_RTCError(RTC_INVALID_ARGUMENT, "rays.instID not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,N,N,N);
    IntersectContext context(scene,user_context);
    scene->device->rayStreamFilters.filterSOP(scene,rays,N,&context,false);
#else
    throw_RTCError(RTC_INVALID_OPERATION,"rtcOccludedNp not supported");
#endif
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcDeleteScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeleteScene);
    RTCORE_VERIFY_HANDLE(hscene);
    delete scene;
    RTCORE_CATCH_END2(scene);
  }

  unsigned rtcNewInstanceImpl (RTCScene htarget, RTCScene hsource, size_t numTimeSteps, unsigned int geomID) 
  {
    Scene* target = (Scene*) htarget;
    Scene* source = (Scene*) hsource;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewInstance);
    RTCORE_VERIFY_HANDLE(htarget);
    RTCORE_VERIFY_HANDLE(hsource);
#if defined(EMBREE_GEOMETRY_USER)
    if (target->device != source->device) throw_RTCError(RTC_INVALID_OPERATION,"scenes do not belong to the same device");
    return target->newInstance(geomID,source,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewInstance is not supported");
#endif
    RTCORE_CATCH_END2(target);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewInstance2 (RTCScene htarget, RTCScene hsource, size_t numTimeSteps) {
    return rtcNewInstanceImpl(htarget,hsource,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewInstance3 (RTCScene htarget, RTCScene hsource, size_t numTimeSteps, unsigned int geomID) {
    return rtcNewInstanceImpl(htarget,hsource,numTimeSteps,geomID);
  }

  RTCORE_API unsigned rtcNewGeometryInstance (RTCScene hscene, unsigned geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewGeometryInstance);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    return scene->newGeometryInstance(RTC_INVALID_GEOMETRY_ID,scene->get_locked(geomID));
    RTCORE_CATCH_END2(scene);
    return -1;
  }

  RTCORE_API unsigned rtcNewGeometryGroup (RTCScene hscene, RTCGeometryFlags gflags, unsigned* geomIDs, size_t N) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewGeometryGroup);
    RTCORE_VERIFY_HANDLE(hscene);
    if (N) RTCORE_VERIFY_HANDLE(geomIDs);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
    std::vector<Geometry*> geometries(N);
    for (size_t i=0; i<N; i++) {
      RTCORE_VERIFY_GEOMID(geomIDs[i]);
      geometries[i] = scene->get_locked(geomIDs[i]);
      if (geometries[i]->getType() == Geometry::GROUP)
        throw_RTCError(RTC_INVALID_ARGUMENT,"geometry groups cannot contain other geometry groups");
      if (geometries[i]->getType() != geometries[0]->getType())
        throw_RTCError(RTC_INVALID_ARGUMENT,"geometries inside group have to be of same type");
    }
    return scene->newGeometryGroup(RTC_INVALID_GEOMETRY_ID,gflags,geometries);
    RTCORE_CATCH_END2(scene);
    return -1;
  }

  AffineSpace3fa convertTransform(RTCMatrixType layout, const float* xfm)
  {
    AffineSpace3fa transform = one;
    switch (layout) 
    {
    case RTC_MATRIX_ROW_MAJOR:
      transform = AffineSpace3fa(Vec3fa(xfm[ 0],xfm[ 4],xfm[ 8]),
                                 Vec3fa(xfm[ 1],xfm[ 5],xfm[ 9]),
                                 Vec3fa(xfm[ 2],xfm[ 6],xfm[10]),
                                 Vec3fa(xfm[ 3],xfm[ 7],xfm[11]));
      break;

    case RTC_MATRIX_COLUMN_MAJOR:
      transform = AffineSpace3fa(Vec3fa(xfm[ 0],xfm[ 1],xfm[ 2]),
                                 Vec3fa(xfm[ 3],xfm[ 4],xfm[ 5]),
                                 Vec3fa(xfm[ 6],xfm[ 7],xfm[ 8]),
                                 Vec3fa(xfm[ 9],xfm[10],xfm[11]));
      break;

    case RTC_MATRIX_COLUMN_MAJOR_ALIGNED16:
      transform = AffineSpace3fa(Vec3fa(xfm[ 0],xfm[ 1],xfm[ 2]),
                                 Vec3fa(xfm[ 4],xfm[ 5],xfm[ 6]),
                                 Vec3fa(xfm[ 8],xfm[ 9],xfm[10]),
                                 Vec3fa(xfm[12],xfm[13],xfm[14]));
      break;

    default: 
      throw_RTCError(RTC_INVALID_OPERATION,"Unknown matrix type");
      break;
    }
    return transform;
  }

  RTCORE_API void rtcSetTransform (RTCScene hscene, unsigned geomID, RTCMatrixType layout, const float* xfm, size_t timeStep) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetTransform);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    RTCORE_VERIFY_HANDLE(xfm);
    const AffineSpace3fa transform = convertTransform(layout,xfm);
    ((Scene*) scene)->get_locked(geomID)->setTransform(transform,timeStep);
    RTCORE_CATCH_END2(scene);
  }

  unsigned rtcNewUserGeometryImpl (RTCScene hscene, RTCGeometryFlags gflags, size_t numItems, size_t numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewUserGeometry2);
    RTCORE_VERIFY_HANDLE(hscene);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_USER)
    return scene->newUserGeometry(geomID,gflags,numItems,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewUserGeometry is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewUserGeometry (RTCScene hscene, RTCGeometryFlags gflags, size_t numItems, size_t numTimeSteps) {
    return rtcNewUserGeometryImpl(hscene,gflags,numItems,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewUserGeometry4 (RTCScene hscene, RTCGeometryFlags gflags, size_t numItems, size_t numTimeSteps, unsigned int geomID) {
    return rtcNewUserGeometryImpl(hscene,gflags,numItems,numTimeSteps,geomID);
  }

  unsigned rtcNewTriangleMeshImpl (RTCScene hscene, RTCGeometryFlags gflags, size_t numTriangles, size_t numVertices, size_t numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewTriangleMesh);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_TRIANGLES)
    return scene->newTriangleMesh(geomID,gflags,numTriangles,numVertices,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewTriangleMesh is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewTriangleMesh (RTCScene hscene, RTCGeometryFlags gflags, size_t numTriangles, size_t numVertices, size_t numTimeSteps) {
    return rtcNewTriangleMeshImpl(hscene,gflags,numTriangles,numVertices,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewTriangleMesh2 (RTCScene hscene, RTCGeometryFlags gflags, size_t numTriangles, size_t numVertices, size_t numTimeSteps, unsigned int geomID) {
    return rtcNewTriangleMeshImpl(hscene,gflags,numTriangles,numVertices,numTimeSteps,geomID);
  }

  unsigned rtcNewQuadMeshImpl(RTCScene hscene, RTCGeometryFlags gflags, size_t numQuads, size_t numVertices, size_t numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewQuadMesh);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_QUADS)
    return scene->newQuadMesh(geomID,gflags,numQuads,numVertices,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewQuadMesh is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewQuadMesh (RTCScene hscene, RTCGeometryFlags gflags, size_t numQuads, size_t numVertices, size_t numTimeSteps) {
    return rtcNewQuadMeshImpl(hscene,gflags,numQuads,numVertices,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewQuadMesh2(RTCScene hscene, RTCGeometryFlags gflags, size_t numQuads, size_t numVertices, size_t numTimeSteps, unsigned int geomID) {
    return rtcNewQuadMeshImpl(hscene,gflags,numQuads,numVertices,numTimeSteps,geomID);
  }

  unsigned rtcNewBezierHairGeometryImpl (RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewBezierHairGeometry);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_HAIR)
    return scene->newCurves(geomID,NativeCurves::HAIR,NativeCurves::BEZIER,gflags,numCurves,numVertices,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewBezierHairGeometry is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewBezierHairGeometry (RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps) {
    return rtcNewBezierHairGeometryImpl(hscene,gflags,(unsigned int)numCurves,(unsigned int)numVertices,(unsigned int)numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewBezierHairGeometry2 (RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) {
    return rtcNewBezierHairGeometryImpl(hscene,gflags,numCurves,numVertices,numTimeSteps,geomID);
  }

  unsigned rtcNewBSplineHairGeometryImpl(RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewBSplineHairGeometry);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_HAIR)
    return scene->newCurves(geomID,NativeCurves::HAIR,NativeCurves::BSPLINE,gflags,numCurves,numVertices,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewBSplineHairGeometry is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }

  RTCORE_API unsigned rtcNewBSplineHairGeometry (RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps) {
    return rtcNewBSplineHairGeometryImpl(hscene,gflags,numCurves,numVertices,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewBSplineHairGeometry2(RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) {
    return rtcNewBSplineHairGeometryImpl(hscene,gflags,numCurves,numVertices,numTimeSteps,geomID);
  }

  unsigned rtcNewBezierCurveGeometryImpl(RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewBezierCurveGeometry);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_HAIR)
    return scene->newCurves(geomID,NativeCurves::SURFACE,NativeCurves::BEZIER,gflags,numCurves,numVertices,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewBezierCurveGeometry is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewBezierCurveGeometry (RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps) {
    return rtcNewBezierCurveGeometryImpl(hscene,gflags,(unsigned int)numCurves,(unsigned int)numVertices,(unsigned int)numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewBezierCurveGeometry2(RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) {
    return rtcNewBezierCurveGeometryImpl(hscene,gflags,numCurves,numVertices,numTimeSteps,geomID);
  }

  unsigned rtcNewBSplineCurveGeometryImpl(RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewBSplineCurveGeometry);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_HAIR)
    return scene->newCurves(geomID,NativeCurves::SURFACE,NativeCurves::BSPLINE,gflags,numCurves,numVertices,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewBSplineCurveGeometry is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewBSplineCurveGeometry (RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps) {
    return rtcNewBSplineCurveGeometryImpl(hscene,gflags,numCurves,numVertices,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewBSplineCurveGeometry2(RTCScene hscene, RTCGeometryFlags gflags, unsigned int numCurves, unsigned int numVertices, unsigned int numTimeSteps, unsigned int geomID) {
    return rtcNewBSplineCurveGeometryImpl(hscene,gflags,numCurves,numVertices,numTimeSteps,geomID);
  }

  unsigned rtcNewLineSegmentsImpl(RTCScene hscene, RTCGeometryFlags gflags, size_t numSegments, size_t numVertices, size_t numTimeSteps, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewLineSegments);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_LINES)
    return scene->newLineSegments(geomID,gflags,numSegments,numVertices,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewLineSegments is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }

  RTCORE_API unsigned rtcNewLineSegments (RTCScene hscene, RTCGeometryFlags gflags, size_t numSegments, size_t numVertices, size_t numTimeSteps) {
    return rtcNewLineSegmentsImpl(hscene,gflags,numSegments,numVertices,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewLineSegments2(RTCScene hscene, RTCGeometryFlags gflags, size_t numSegments, size_t numVertices, size_t numTimeSteps, unsigned int geomID) {
    return rtcNewLineSegmentsImpl(hscene,gflags,numSegments,numVertices,numTimeSteps,geomID);
  }

  unsigned rtcNewSubdivisionMeshImpl(RTCScene hscene, RTCGeometryFlags gflags, size_t numFaces, size_t numEdges, size_t numVertices, 
                                         size_t numEdgeCreases, size_t numVertexCreases, size_t numHoles, size_t numTimeSteps, unsigned int geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcNewSubdivisionMesh);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_RANGE(numTimeSteps,1,RTC_MAX_TIME_STEPS);
    if (scene->isStatic() && (gflags != RTC_GEOMETRY_STATIC))
      throw_RTCError(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
#if defined(EMBREE_GEOMETRY_SUBDIV)
    return scene->newSubdivisionMesh(geomID,gflags,numFaces,numEdges,numVertices,numEdgeCreases,numVertexCreases,numHoles,numTimeSteps);
#else
    throw_RTCError(RTC_UNKNOWN_ERROR,"rtcNewSubdivisionMesh is not supported");
#endif
    RTCORE_CATCH_END2(scene);
    return -1;
  }
  
  RTCORE_API unsigned rtcNewSubdivisionMesh (RTCScene hscene, RTCGeometryFlags gflags, size_t numFaces, size_t numEdges, size_t numVertices, 
                                             size_t numEdgeCreases, size_t numVertexCreases, size_t numHoles, size_t numTimeSteps) 
  {
    return rtcNewSubdivisionMeshImpl(hscene,gflags,numFaces,numEdges,numVertices,numEdgeCreases,numVertexCreases,numHoles,numTimeSteps,RTC_INVALID_GEOMETRY_ID);
  }

  RTCORE_API unsigned rtcNewSubdivisionMesh2(RTCScene hscene, RTCGeometryFlags gflags, size_t numFaces, size_t numEdges, size_t numVertices, 
                                             size_t numEdgeCreases, size_t numVertexCreases, size_t numHoles, size_t numTimeSteps, unsigned int geomID) 
  {
    return rtcNewSubdivisionMeshImpl(hscene,gflags,numFaces,numEdges,numVertices,numEdgeCreases,numVertexCreases,numHoles,numTimeSteps,geomID);
  }
  
  RTCORE_API void rtcSetMask (RTCScene hscene, unsigned geomID, int mask) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetMask);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setMask(mask);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetSubdivisionMode (RTCScene hscene, unsigned geomID, unsigned topologyID, RTCSubdivisionMode mode) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetSubdivisionMode);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setSubdivisionMode(topologyID,mode);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIndexBuffer (RTCScene hscene, unsigned geomID, RTCBufferType vertexBuffer, RTCBufferType indexBuffer) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIndexBuffer);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIndexBuffer(vertexBuffer,indexBuffer);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void* rtcMapBuffer(RTCScene hscene, unsigned geomID, RTCBufferType type) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcMapBuffer);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    return scene->get_locked(geomID)->map(type);
    RTCORE_CATCH_END2(scene);
    return nullptr;
  }

  RTCORE_API void rtcUnmapBuffer(RTCScene hscene, unsigned geomID, RTCBufferType type) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcUnmapBuffer);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->unmap(type);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetBuffer(RTCScene hscene, unsigned geomID, RTCBufferType type, const void* ptr, size_t offset, size_t stride, size_t size)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetBuffer);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    RTCORE_VERIFY_UPPER(stride,unsigned(inf));
    scene->get_locked(geomID)->setBuffer(type,(void*)ptr,offset,stride,size);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcEnable (RTCScene hscene, unsigned geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcEnable);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->enable();
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcUpdate (RTCScene hscene, unsigned geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcUpdate);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->update();
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcUpdateBuffer (RTCScene hscene, unsigned geomID, RTCBufferType type) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcUpdateBuffer);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->updateBuffer(type);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcDisable (RTCScene hscene, unsigned geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDisable);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->disable();
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcDeleteGeometry (RTCScene hscene, unsigned geomID) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcDeleteGeometry);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->deleteGeometry(geomID);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetTessellationRate (RTCScene hscene, unsigned geomID, float tessellationRate)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetTessellationRate);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setTessellationRate(tessellationRate);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetUserData (RTCScene hscene, unsigned geomID, void* ptr) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetUserData);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setUserData(ptr);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void* rtcGetUserData (RTCScene hscene, unsigned geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcGetUserData);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    return scene->get(geomID)->getUserData(); // this call is on purpose not thread safe
    RTCORE_CATCH_END2(scene);
    return nullptr;
  }

  RTCORE_API void rtcSetBoundsFunction (RTCScene hscene, unsigned geomID, RTCBoundsFunc bounds, void* userPtr)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetBoundsFunction);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setBoundsFunction(bounds,userPtr);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetDisplacementFunction (RTCScene hscene, unsigned geomID, RTCDisplacementFunc func, RTCBounds* bounds)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetDisplacementFunction);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setDisplacementFunction(func,bounds);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectFunction (RTCScene hscene, unsigned geomID, RTCIntersectFunc intersect) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectFunction);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectFunction(intersect);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectFunction4 (RTCScene hscene, unsigned geomID, RTCIntersectFunc4 intersect4) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectFunction4);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectFunction4(intersect4);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectFunction8 (RTCScene hscene, unsigned geomID, RTCIntersectFunc8 intersect8) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectFunction8);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectFunction8(intersect8);
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcSetIntersectFunction16 (RTCScene hscene, unsigned geomID, RTCIntersectFunc16 intersect16) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectFunction16);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectFunction16(intersect16);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectFunction1Mp (RTCScene hscene, unsigned geomID, RTCIntersectFunc1Mp intersect) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectFunction1Mp);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectFunction1Mp(intersect);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectFunctionN (RTCScene hscene, unsigned geomID, RTCIntersectFuncN intersect) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectFunctionN);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectFunctionN(intersect);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOccludedFunction (RTCScene hscene, unsigned geomID, RTCOccludedFunc occluded) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOccludedFunction);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOccludedFunction(occluded);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOccludedFunction4 (RTCScene hscene, unsigned geomID, RTCOccludedFunc4 occluded4) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOccludedFunction4);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOccludedFunction4(occluded4);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOccludedFunction8 (RTCScene hscene, unsigned geomID, RTCOccludedFunc8 occluded8) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOccludedFunction8);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOccludedFunction8(occluded8);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOccludedFunction16 (RTCScene hscene, unsigned geomID, RTCOccludedFunc16 occluded16) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOccludedFunction16);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOccludedFunction16(occluded16);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOccludedFunction1Mp (RTCScene hscene, unsigned geomID, RTCOccludedFunc1Mp occluded) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOccludedFunction1Mp);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOccludedFunction1Mp(occluded);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOccludedFunctionN (RTCScene hscene, unsigned geomID, RTCOccludedFuncN occluded) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOccludedFunctionN);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOccludedFunctionN(occluded);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectionFilterFunction (RTCScene hscene, unsigned geomID, RTCFilterFunc intersect) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectionFilterFunction);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectionFilterFunction(intersect);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectionFilterFunction4 (RTCScene hscene, unsigned geomID, RTCFilterFunc4 filter4) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectionFilterFunction4);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectionFilterFunction4(filter4);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectionFilterFunction8 (RTCScene hscene, unsigned geomID, RTCFilterFunc8 filter8) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectionFilterFunction8);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectionFilterFunction8(filter8);
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcSetIntersectionFilterFunction16 (RTCScene hscene, unsigned geomID, RTCFilterFunc16 filter16) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectionFilterFunction16);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectionFilterFunction16(filter16);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetIntersectionFilterFunctionN (RTCScene hscene, unsigned geomID, RTCFilterFuncN filterN) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetIntersectionFilterFunctionN);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setIntersectionFilterFunctionN(filterN);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOcclusionFilterFunction (RTCScene hscene, unsigned geomID, RTCFilterFunc intersect) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOcclusionFilterFunction);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOcclusionFilterFunction(intersect);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOcclusionFilterFunction4 (RTCScene hscene, unsigned geomID, RTCFilterFunc4 filter4) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOcclusionFilterFunction4);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOcclusionFilterFunction4(filter4);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOcclusionFilterFunction8 (RTCScene hscene, unsigned geomID, RTCFilterFunc8 filter8) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOcclusionFilterFunction8);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOcclusionFilterFunction8(filter8);
    RTCORE_CATCH_END2(scene);
  }
  
  RTCORE_API void rtcSetOcclusionFilterFunction16 (RTCScene hscene, unsigned geomID, RTCFilterFunc16 filter16) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOcclusionFilterFunction16);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOcclusionFilterFunction16(filter16);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcSetOcclusionFilterFunctionN (RTCScene hscene, unsigned geomID, RTCFilterFuncN filterN) 
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcSetOcclusionFilterFunctionN);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get_locked(geomID)->setOcclusionFilterFunctionN(filterN);
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcInterpolate(RTCScene hscene, unsigned geomID, unsigned primID, float u, float v, 
                                 RTCBufferType buffer,
                                 float* P, float* dPdu, float* dPdv, size_t numFloats)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcInterpolate);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get(geomID)->interpolate(primID,u,v,buffer,P,dPdu,dPdv,nullptr,nullptr,nullptr,numFloats); // this call is on purpose not thread safe
    RTCORE_CATCH_END2(scene);
  }

  RTCORE_API void rtcInterpolate2(RTCScene hscene, unsigned geomID, unsigned primID, float u, float v, 
                                  RTCBufferType buffer,
                                  float* P, float* dPdu, float* dPdv, 
                                  float* ddPdudu, float* ddPdvdv, float* ddPdudv, 
                                  size_t numFloats)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcInterpolate);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get(geomID)->interpolate(primID,u,v,buffer,P,dPdu,dPdv,ddPdudu,ddPdvdv,ddPdudv,numFloats); // this call is on purpose not thread safe
    RTCORE_CATCH_END2(scene);
  }


#if defined (EMBREE_RAY_PACKETS)
  RTCORE_API void rtcInterpolateN(RTCScene hscene, unsigned geomID, 
                                  const void* valid_i, const unsigned* primIDs, const float* u, const float* v, size_t numUVs, 
                                  RTCBufferType buffer,
                                  float* P, float* dPdu, float* dPdv, size_t numFloats)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcInterpolateN);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get(geomID)->interpolateN(valid_i,primIDs,u,v,numUVs,buffer,P,dPdu,dPdv,nullptr,nullptr,nullptr,numFloats); // this call is on purpose not thread safe
    RTCORE_CATCH_END2(scene);
  }
#endif

#if defined (EMBREE_RAY_PACKETS)
  RTCORE_API void rtcInterpolateN2(RTCScene hscene, unsigned geomID, 
                                   const void* valid_i, const unsigned* primIDs, const float* u, const float* v, size_t numUVs, 
                                   RTCBufferType buffer,
                                   float* P, float* dPdu, float* dPdv, 
                                   float* ddPdudu, float* ddPdvdv, float* ddPdudv, 
                                   size_t numFloats)
  {
    Scene* scene = (Scene*) hscene;
    RTCORE_CATCH_BEGIN;
    RTCORE_TRACE(rtcInterpolateN);
    RTCORE_VERIFY_HANDLE(hscene);
    RTCORE_VERIFY_GEOMID(geomID);
    scene->get(geomID)->interpolateN(valid_i,primIDs,u,v,numUVs,buffer,P,dPdu,dPdv,ddPdudu,ddPdvdv,ddPdudv,numFloats); // this call is on purpose not thread safe
    RTCORE_CATCH_END2(scene);
  }
#endif
}
