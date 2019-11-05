/*
 * Copyright (c) 1999-2012 Mark D. Hill and David A. Wood
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_RUBY_STRUCTURES_CACHEMEMORY_HH__
#define __MEM_RUBY_STRUCTURES_CACHEMEMORY_HH__

#include <string>
#include <unordered_map>
#include <tuple>

#include <vector>
#include <string>

#include "base/statistics.hh"
#include "mem/protocol/CacheRequestType.hh"
#include "mem/protocol/CacheResourceType.hh"
#include "mem/protocol/RubyRequest.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/slicc_interface/AbstractCacheEntry.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "mem/ruby/structures/AbstractReplacementPolicy.hh"
#include "mem/ruby/structures/BankedArray.hh"
#include "mem/ruby/system/CacheRecorder.hh"
#include "params/RubyCache.hh"
#include "mem/packet.hh"
#include "sim/sim_object.hh"

typedef std::tuple<Addr,int64_t,int> SideEffectEntry; //<EvictedPhyAddr, EvictedCacheSet, EvictedWay>
typedef std::map<Addr,SideEffectEntry> SideEffectTable; //<InstalledAddr, EvictedPhyAddr>
typedef std::pair<const Addr, SideEffectEntry> value_type_SET;

typedef enum CleanupType {inflight, executed} CleanupType;

typedef struct {
  Addr Rollback_LineAddr;
  bool toEvict_L1;
  bool toInstall_L1;
  bool toEvict_L2;
  bool doneEvict_L1;
  bool doneInstall_L1;
  bool doneEvict_L2;
  PacketPtr cleanupPkt;
  CleanupType cleanup_type;
  int ct_id;
  Cycles load_completion_tick;
} CleanupEntry;

typedef std::pair<Addr,CleanupEntry> CleanupTableEntry;
typedef std::vector<CleanupTableEntry> CleanupTable;

class CacheMemory : public SimObject
{
 public:
  typedef RubyCacheParams Params;
  CacheMemory(const Params *p);
  ~CacheMemory();

  void init();

  // Public Methods
  // perform a cache access and see if we hit or not.  Return true on a hit.
  bool tryCacheAccess(Addr address, RubyRequestType type,
                      DataBlock*& data_ptr);

  // similar to above, but doesn't require full access check
  bool testCacheAccess(Addr address, RubyRequestType type,
                       DataBlock*& data_ptr);

  // tests to see if an address is present in the cache
  bool isTagPresent(Addr address) const;

  // Returns true if there is:
  //   a) a tag match on this address or there is
  //   b) an unused line in the same cache "way"
  bool cacheAvail(Addr address) const;

  // find an unused entry and sets the tag appropriate for the address
  AbstractCacheEntry* allocate(Addr address,
                               AbstractCacheEntry* new_entry, bool touch);
  AbstractCacheEntry* allocate(Addr address, AbstractCacheEntry* new_entry)
  {
    return allocate(address, new_entry, true);
  }
  void allocateVoid(Addr address, AbstractCacheEntry* new_entry)
  {
    allocate(address, new_entry, true);
  }

  // Explicitly free up this address
  void deallocate(Addr address);

  // Returns with the physical address of the conflicting cache line
  Addr cacheProbe(Addr address, bool track_sideeffects) ;

  // looks an address up in the cache
  AbstractCacheEntry* lookup(Addr address);
  const AbstractCacheEntry* lookup(Addr address) const;

  Cycles getTagLatency() const { return tagArray.getLatency(); }
  Cycles getDataLatency() const { return dataArray.getLatency(); }

  bool isBlockInvalid(int64_t cache_set, int64_t loc);
  bool isBlockNotBusy(int64_t cache_set, int64_t loc);

  // Hook for checkpointing the contents of the cache
  void recordCacheContents(int cntrl, CacheRecorder* tr) const;

  // Set this address to most recently used
  void setMRU(Addr address);
  void setMRU(Addr addr, int occupancy);
  int getReplacementWeight(int64_t set, int64_t loc);
  void setMRU(const AbstractCacheEntry *e);

  // Functions for locking and unlocking cache lines corresponding to the
  // provided address.  These are required for supporting atomic memory
  // accesses.  These are to be used when only the address of the cache entry
  // is available.  In case the entry itself is available. use the functions
  // provided by the AbstractCacheEntry class.
  void setLocked (Addr addr, int context);
  void clearLocked (Addr addr);
  bool isLocked (Addr addr, int context);

  // Print cache contents
  void print(std::ostream& out) const;
  void printData(std::ostream& out) const;

  void regStats();
  bool checkResourceAvailable(CacheResourceType res, Addr addr);
  void recordRequestType(CacheRequestType requestType, Addr addr);

 public:
  Stats::Scalar m_demand_hits;
  Stats::Scalar m_demand_misses;
  Stats::Formula m_demand_accesses;

  Stats::Scalar m_sw_prefetches;
  Stats::Scalar m_hw_prefetches;
  Stats::Formula m_prefetches;

  Stats::Vector m_accessModeType;

  Stats::Scalar numDataArrayReads;
  Stats::Scalar numDataArrayWrites;
  Stats::Scalar numTagArrayReads;
  Stats::Scalar numTagArrayWrites;

  Stats::Scalar numTagArrayStalls;
  Stats::Scalar numDataArrayStalls;

  int getCacheSize() const { return m_cache_size; }
  int getCacheAssoc() const { return m_cache_assoc; }
  int getNumBlocks() const { return m_cache_num_sets * m_cache_assoc; }
  Addr getAddressAtIdx(int idx) const;

  // [CleanupCache] For remapping lines every n accesses.
  bool needs_remap(); //Checks if remap is to be done at this time.
  Addr get_remap_addr(); // Models remapping a line.
  int get_demand_accesses();
  // Return the number of demand accesses to cache.
  void reset_remap_var(); // Reset status bit
  //(m_issued_remap) when access_count updated.
  // Remove side-effect record from MSHR on completion of miss
  SideEffectEntry getMSHRentry(Addr address);
  Addr removeMSHRentry(Addr address);
  bool isAddrValid (Addr address);
    
  //[CleanupCache]: For tracking cleanups
  int cleanup_table_insert(Addr Cleanup_LineAddr, CleanupEntry cleanup_entry) ;
  bool cleanup_table_erase(Addr Cleanup_LineAddr,int ct_id );
  CleanupEntry cleanup_table_find(Addr Cleanup_LineAddr, int ct_id);
  CleanupTable::iterator cleanup_table_findit(Addr Cleanup_LineAddr, int ct_id);  
  bool cleanup_needs_evictL1(Addr Cleanup_LineAddr, int ct_id);
  bool cleanup_needs_rollback(Addr Cleanup_LineAddr, int ct_id);
  Addr cleanup_get_rollback_addr(Addr Cleanup_LineAddrr, int ct_id);
  Addr cleanup_get_cleanup_addr(Addr Rollback_LineAddrr, int ct_id);
  bool cleanup_needs_evictL2(Addr Cleanup_LineAddrr, int ct_id);
  bool cleanup_done(Addr Cleanup_LineAddr,int ct_id);
  bool cleanup_set_toEvictL1(Addr Cleanup_LineAddr, bool value,int ct_id);
  bool cleanup_set_toEvictL2(Addr Cleanup_LineAddr, bool value,int ct_id);
  bool cleanup_set_toInstallL1(Addr Cleanup_LineAddr, bool value,int ct_id);
  bool cleanup_set_doneEvictL1(Addr Cleanup_LineAddr, bool value,int ct_id);
  bool cleanup_set_doneEvictL2(Addr Cleanup_LineAddr, bool value,int ct_id);
  bool cleanup_set_doneInstallL1(Addr Cleanup_LineAddr, bool value,int ct_id);
  bool cleanup_check_addr_involved(Addr LineAddr,int ct_id);
  bool cleanup_pending_rollback(Addr LineAddr,int ct_id);
  bool cleanup_prev_to_set(Addr LineAddr,int ct_id);
  int  cleanup_table_size();
  void cleanup_table_dumpcontents(std::string ctx);
  void cleanup_pending_count_incr(Addr line_addr); //Increments cleanups seen by sequencer.
  void cleanup_pending_count_decr(Addr line_addr); //Decrements cleanups seen by sequencer.
  bool cleanup_issue_pending();
  bool cleanup_datareq_dependancy(Addr data_line_addr);
  bool cleanup_inflight_received(Addr data_line_addr);
  DataBlock& getConstDataBlock();
  
private:
  CleanupTable::iterator find_ct(Addr LineAddr,int ct_id);
  int next_ct_id;
  //AbstractCacheEntry* constCacheEntry;
  DataBlock constDataBlk;
  
private:
  // convert a Address to its location in the cache
  int64_t addressToCacheSet(Addr address) const;

  // Given a cache tag: returns the index of the tag in a set.
  // returns -1 if the tag is not found.
  int findTagInSet(int64_t line, Addr tag) const;
  int findTagInSetIgnorePermissions(int64_t cacheSet, Addr tag) const;

  // Private copy constructor and assignment operator
  CacheMemory(const CacheMemory& obj);
  CacheMemory& operator=(const CacheMemory& obj);
  
  // [CleanupCache] For generating random mapping table for randomized caches
  void gen_rand_table (std::vector<int64_t>                     \
                       &m_ela_table, int64_t num_lines_in_mem);

 private:
  // Data Members (m_prefix)
  bool m_is_instruction_only_cache;
  //[CleanupCache]
  bool m_is_index_randomized;
  int m_remap_accesses; //N accesses after which remap called
  bool m_issued_remap;
  bool m_tracks_sideeffects;    
  SideEffectTable mshr; // <Miss_Address, <Side-Effect i.e. Evicted Address, Evicted Set, Evicted_Way>
  CleanupTable ct; //<Cleanup_LineAddr, CleanupEntry>
  int64_t cleanup_table_count; //counts entries in the cleanup table.
  int pending_cleanup_count; //counts cleanup packets seen by sequencer.
  
  int m_mem_footprint_bits;
  std::vector<int64_t> m_ela_table; // For mapping of PLA->ELA
  std::vector<int64_t> m_inv_ela_table; // For inverse mapping of ELA->PLA
  
  // The first index is the # of cache lines.
  // The second index is the the amount associativity.
  std::unordered_map<Addr, int> m_tag_index;
  std::vector<std::vector<AbstractCacheEntry*> > m_cache;
      
  AbstractReplacementPolicy *m_replacementPolicy_ptr;

  BankedArray dataArray;
  BankedArray tagArray;
  
  int m_cache_size;
  int m_cache_num_sets;
  int m_cache_num_set_bits;
  int m_cache_assoc;
  int m_start_index_bit;
  bool m_resource_stalls;
  int m_block_size;
};

std::ostream& operator<<(std::ostream& out, const CacheMemory& obj);

#endif // __MEM_RUBY_STRUCTURES_CACHEMEMORY_HH__
