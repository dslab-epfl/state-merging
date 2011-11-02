#ifndef KLEE_QCE_H
#define KLEE_QCE_H

#include "llvm/Value.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/FoldingSet.h"

#define QCE_BWIDTH 128

namespace llvm {
  class raw_ostream;
}

namespace klee {

enum HotValueKind { HVVal, HVPtr };
typedef llvm::PointerIntPair<llvm::Value*, 1, HotValueKind> HotValueBaseTy;

class HotValue: public HotValueBaseTy {
public:
  HotValue(): PointerIntPair(NULL, HVVal) {}
  HotValue(HotValueKind K, llvm::Value *V): PointerIntPair(V, K) {}

  llvm::Value *getValue() const { return getPointer(); }
  HotValueKind getKind() const { return getInt(); }

  bool isVal() const { return getInt() == HVVal; }
  bool isPtr() const { return getInt() == HVPtr; }

  // An easy way to support DenseMap
  HotValue(const HotValueBaseTy& P): PointerIntPair(P) {}
  operator HotValueBaseTy () {
    return HotValueBaseTy::getFromOpaqueValue(getOpaqueValue());
  }

  void print(llvm::raw_ostream& ostr) const;
  void dump() const;
};

} // namespace klee

namespace llvm {
  template<> struct FoldingSetTrait<klee::HotValue> {
    static inline void Profile(const klee::HotValue hv, FoldingSetNodeID& ID) {
      ID.AddPointer(hv.getOpaqueValue());
    }
  };
  template<> struct PointerLikeTypeTraits<klee::HotValue>:
    public PointerLikeTypeTraits<klee::HotValueBaseTy> {};
  template<> struct DenseMapInfo<klee::HotValue>:
    public DenseMapInfo<klee::HotValueBaseTy> {};
} // namespace llvm

#endif // KLEE_QCE_H
