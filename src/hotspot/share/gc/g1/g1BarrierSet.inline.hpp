/*
 * Copyright (c) 2016, 2020, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_GC_G1_G1BARRIERSET_INLINE_HPP
#define SHARE_GC_G1_G1BARRIERSET_INLINE_HPP

#include "gc/g1/g1BarrierSet.hpp"

#include "gc/g1/g1CardTable.hpp"
#include "gc/g1/g1ThreadLocalData.hpp"
#include "gc/shared/accessBarrierSupport.inline.hpp"
#include "oops/access.inline.hpp"
#include "oops/compressedOops.inline.hpp"
#include "oops/oop.hpp"

inline void G1BarrierSet::enqueue_preloaded(oop pre_val) {
  // Nulls should have been already filtered.
  assert(oopDesc::is_oop(pre_val, true), "Error");

  G1SATBMarkQueueSet& queue_set = G1BarrierSet::satb_mark_queue_set();
  if (!queue_set.is_active()) return;

  SATBMarkQueue& queue = G1ThreadLocalData::satb_mark_queue(Thread::current());
  queue_set.enqueue_known_active(queue, pre_val);
}

template <class T>
inline void G1BarrierSet::enqueue(T* dst) {
   //!satb-enqueue -2 全局的 satb queue。
  G1SATBMarkQueueSet& queue_set = G1BarrierSet::satb_mark_queue_set();
  if (!queue_set.is_active()) return;

  T heap_oop = RawAccess<MO_RELAXED>::oop_load(dst);
  if (!CompressedOops::is_null(heap_oop)) {
      //!satb-enqueue -3 取到local satb queue
    SATBMarkQueue& queue = G1ThreadLocalData::satb_mark_queue(Thread::current());
    queue_set.enqueue_known_active(queue, CompressedOops::decode_not_null(heap_oop));
  }
}

template <DecoratorSet decorators, typename T>
inline void G1BarrierSet::write_ref_field_pre(T* field) {
  if (HasDecorator<decorators, IS_DEST_UNINITIALIZED>::value ||
      HasDecorator<decorators, AS_NO_KEEPALIVE>::value) {
    return;
  }
//!satb-enqueue -1
//!xiaojin-mark satb -1 在写屏障pre阶段会将快照obj放入 satb 队列，后续concurrent marking阶段会处理。
  enqueue(field);
}

template <DecoratorSet decorators, typename T>
inline void G1BarrierSet::write_ref_field_post(T* field, oop new_val) {
    //! new_val 是年轻对象，filed是老年对象。也就是老年代对象引用了年轻代对象。
  volatile CardValue* byte = _card_table->byte_for(field);
  //! 从card表中可以看出某个对象是否是old。
  if (*byte != G1CardTable::g1_young_card_val()) {
    // Take a slow path for cards in old
    //!xiaojin-cardtable write barrier 把card表中这个对象的那个卡位置的地址写入了脏卡queue。有了个byte就知道了这个对象在card表中的位置，就可以计算出对象的指针。
    write_ref_field_post_slow(byte);
  }
}

inline void G1BarrierSet::enqueue_preloaded_if_weak(DecoratorSet decorators, oop value) {
  assert((decorators & ON_UNKNOWN_OOP_REF) == 0, "Reference strength must be known");
  // Loading from a weak or phantom reference needs enqueueing, as
  // the object may not have been reachable (part of the snapshot)
  // when marking started.
  const bool on_strong_oop_ref = (decorators & ON_STRONG_OOP_REF) != 0;
  const bool peek              = (decorators & AS_NO_KEEPALIVE) != 0;
  const bool needs_enqueue     = (!peek && !on_strong_oop_ref);

  if (needs_enqueue && value != NULL) {
    enqueue_preloaded(value);
  }
}

template <DecoratorSet decorators, typename BarrierSetT>
template <typename T>
inline oop G1BarrierSet::AccessBarrier<decorators, BarrierSetT>::
oop_load_not_in_heap(T* addr) {
  oop value = ModRef::oop_load_not_in_heap(addr);
  enqueue_preloaded_if_weak(decorators, value);
  return value;
}

template <DecoratorSet decorators, typename BarrierSetT>
template <typename T>
inline oop G1BarrierSet::AccessBarrier<decorators, BarrierSetT>::
oop_load_in_heap(T* addr) {
  oop value = ModRef::oop_load_in_heap(addr);
  enqueue_preloaded_if_weak(decorators, value);
  return value;
}

template <DecoratorSet decorators, typename BarrierSetT>
inline oop G1BarrierSet::AccessBarrier<decorators, BarrierSetT>::
oop_load_in_heap_at(oop base, ptrdiff_t offset) {
  oop value = ModRef::oop_load_in_heap_at(base, offset);
  enqueue_preloaded_if_weak(AccessBarrierSupport::resolve_possibly_unknown_oop_ref_strength<decorators>(base, offset), value);
  return value;
}

template <DecoratorSet decorators, typename BarrierSetT>
template <typename T>
inline void G1BarrierSet::AccessBarrier<decorators, BarrierSetT>::
oop_store_not_in_heap(T* addr, oop new_value) {
  // Apply SATB barriers for all non-heap references, to allow
  // concurrent scanning of such references.
  G1BarrierSet *bs = barrier_set_cast<G1BarrierSet>(BarrierSet::barrier_set());
  bs->write_ref_field_pre<decorators>(addr);
  Raw::oop_store(addr, new_value);
}

#endif // SHARE_GC_G1_G1BARRIERSET_INLINE_HPP
