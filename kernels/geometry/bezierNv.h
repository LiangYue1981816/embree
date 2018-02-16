// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
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

#pragma once

#include "primitive.h"
#include "bezierNi.h"

namespace embree
{
  template<int M>
    struct BezierNv : public BezierNi<M>
  {
    using BezierNi<M>::N;
      
    struct Type : public PrimitiveType {
      Type ();
      size_t size(const char* This) const;
      size_t getBytes(const char* This) const;
    };
    static Type type;

  public:

    /* Returns maximum number of stored primitives */
    static __forceinline size_t max_size() { return M; }

    /* Returns required number of primitive blocks for N primitives */
    static __forceinline size_t blocks(size_t N) { return (N+M-1)/M; }

    static __forceinline size_t bytes(size_t N)
    {
      const size_t f = N/M, r = N%M;
      static_assert(sizeof(BezierNv) == 22+25*M+4*16*M, "internal data layout issue");
      return f*sizeof(BezierNv) + (r!=0)*(22 + 25*r + 4*16*r);
    }

  public:

    /*! Default constructor. */
    __forceinline BezierNv () {}

    /*! fill curve from curve list */
    __forceinline void fill(const PrimRef* prims, size_t& begin, size_t _end, Scene* scene)
    {
      size_t end = min(begin+M,_end);
      size_t N = end-begin;

      /* encode all primitives */
      for (size_t i=0; i<N; i++)
      {
        const PrimRef& prim = prims[begin+i];
        const unsigned int geomID = prim.geomID();
        const unsigned int primID = prim.primID();
        CurveGeometry* mesh = (CurveGeometry*) scene->get(geomID);
        const unsigned vtxID = mesh->curve(primID);
        Vec3fa::storeu(&this->vertices(i,N)[0],mesh->vertex(vtxID+0));
        Vec3fa::storeu(&this->vertices(i,N)[1],mesh->vertex(vtxID+1));
        Vec3fa::storeu(&this->vertices(i,N)[2],mesh->vertex(vtxID+2));
        Vec3fa::storeu(&this->vertices(i,N)[3],mesh->vertex(vtxID+3));
      }
    }

    template<typename BVH, typename Allocator>
      __forceinline static typename BVH::NodeRef createLeaf (BVH* bvh, const PrimRef* prims, const range<size_t>& set, const Allocator& alloc)
    {
      size_t start = set.begin();
      size_t items = BezierNv::blocks(set.size());
      size_t numbytes = BezierNv::bytes(set.size());
      BezierNv* accel = (BezierNv*) alloc.malloc1(numbytes,BVH::byteAlignment);
      for (size_t i=0; i<items; i++) {
        accel[i].BezierNv<M>::fill(prims,start,set.end(),bvh->scene);
        accel[i].BezierNi<M>::fill(prims,start,set.end(),bvh->scene);
      }
      return bvh->encodeLeaf((char*)accel,items);
    };
    
  public:
    unsigned char data[4*16*M];
    __forceinline       Vec3fa* vertices(size_t i, size_t N)       { return (Vec3fa*)BezierNi<M>::end(N)+4*i; }
    __forceinline const Vec3fa* vertices(size_t i, size_t N) const { return (Vec3fa*)BezierNi<M>::end(N)+4*i; }
  };

  template<int M>
    typename BezierNv<M>::Type BezierNv<M>::type;

  typedef BezierNv<4> Bezier4v;
  typedef BezierNv<8> Bezier8v;
}