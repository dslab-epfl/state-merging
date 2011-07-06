//===-- Statistic.h ---------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_STATISTIC_H
#define KLEE_STATISTIC_H

#include "klee/Config/config.h"
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/Support/DataTypes.h"
#else
#include "llvm/System/DataTypes.h"
#endif
#else
#include "llvm/Support/DataTypes.h"
#endif
#include <string>

namespace klee {
  class Statistic;
  class StatisticManager;
  class StatisticRecord;

  /// Statistic - A named statistic instance.
  ///
  /// The Statistic class holds information about the statistic, but
  /// not the actual values. Values are managed by the global
  /// StatisticManager to enable transparent support for instruction
  /// level and call path level statistics.
  class Statistic {
    friend class StatisticManager;
    friend class StatisticRecord;

  private:
    unsigned id;
    const std::string name;
    const std::string shortName;
    bool m_isTime;

  public:
    Statistic(const std::string &_name, 
              const std::string &_shortName,
              bool _isTime = false);
    ~Statistic();

    /// getID - Get the unique statistic ID.
    unsigned getID() { return id; }

    /// getName - Get the statistic name.
    const std::string &getName() const { return name; }

    /// getShortName - Get the "short" statistic name, used in
    /// callgrind output for example.
    const std::string &getShortName() const { return shortName; }

    /// getValue - Get the current primary statistic value.
    uint64_t getValue() const;

    void setValue(uint64_t value);

    /// isTime - return true is this statistic describes time
    bool isTime() const { return m_isTime; }

    /// operator uint64_t - Get the current primary statistic value.
    operator uint64_t () const { return getValue(); }

    /// operator++ - Increment the statistic by 1.
    Statistic &operator ++() { return (*this += 1); }

    /// operator+= - Increment the statistic by \arg addend.
    Statistic &operator +=(const uint64_t addend);
  };
}

#endif

