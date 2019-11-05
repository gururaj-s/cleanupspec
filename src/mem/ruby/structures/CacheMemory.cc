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

#include "mem/ruby/structures/CacheMemory.hh"

#include "base/intmath.hh"
#include "debug/RubyCache.hh"
#include "debug/RubyCacheTrace.hh"
#include "debug/RubyResourceStalls.hh"
#include "debug/RubyStats.hh"
#include "mem/protocol/AccessPermission.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "mem/ruby/system/WeightedLRUPolicy.hh"
#include "sim/backtrace.hh"

using namespace std;

ostream&
operator<<(ostream& out, const CacheMemory& obj)
{
    obj.print(out);
    out << flush;
    return out;
}

CacheMemory *
RubyCacheParams::create()
{
    return new CacheMemory(this);
}

CacheMemory::CacheMemory(const Params *p)
    : SimObject(p),
    dataArray(p->dataArrayBanks, p->dataAccessLatency,
              p->start_index_bit, p->ruby_system),
    tagArray(p->tagArrayBanks, p->tagAccessLatency,
             p->start_index_bit, p->ruby_system)
{
    m_cache_size = p->size;
    m_cache_assoc = p->assoc;
    m_replacementPolicy_ptr = p->replacement_policy;
    m_replacementPolicy_ptr->setCache(this);
    m_start_index_bit = p->start_index_bit;
    m_is_instruction_only_cache = p->is_icache;
    m_resource_stalls = p->resourceStalls;
    m_block_size = p->block_size;  // may be 0 at this point. Updated in init()
    //[CleanupCache]
    m_is_index_randomized = p->is_index_randomized;
    m_remap_accesses = p->remap_accesses;
    m_tracks_sideeffects = p->tracks_sideeffects;
    m_mem_footprint_bits = p->mem_footprint_bits;
    //cprintf("Size: %lld, Assoc: %d, Icache:%d\n\n",m_cache_size,m_cache_assoc,m_is_instruction_only_cache);
    
}

void
CacheMemory::init()
{
    if (m_block_size == 0) {
        m_block_size = RubySystem::getBlockSizeBytes();
    }
    m_cache_num_sets = (m_cache_size / m_cache_assoc) / m_block_size;
    assert(m_cache_num_sets > 1);
    m_cache_num_set_bits = floorLog2(m_cache_num_sets);
    assert(m_cache_num_set_bits > 0);
    m_cache.resize(m_cache_num_sets,
                   std::vector<AbstractCacheEntry*>(m_cache_assoc, nullptr));

    // [CleanupCache] Generate RandomMapping Table
    srand(42);

    if (m_is_index_randomized){
      assert(m_mem_footprint_bits > 0);
      assert((1 <<m_start_index_bit) == m_block_size);
      int64_t num_lines_in_mem =\
        (1 << (m_mem_footprint_bits - m_start_index_bit)) ;
      gen_rand_table(m_ela_table, num_lines_in_mem);
    }

    // [CleanupCache] Initialize the status bit for remap
    m_issued_remap = 0;
    cleanup_table_count = 0;
    pending_cleanup_count=0;
    next_ct_id = 0;
}

CacheMemory::~CacheMemory()
{
    if (m_replacementPolicy_ptr)
        delete m_replacementPolicy_ptr;
    for (int i = 0; i < m_cache_num_sets; i++) {
        for (int j = 0; j < m_cache_assoc; j++) {
            delete m_cache[i][j];
        }
    }
}

// convert a Address to its location in the cache
int64_t
CacheMemory::addressToCacheSet(Addr address) const
{
  int64_t cacheset = -1;
    assert(address == makeLineAddress(address));

    //[CleanupCache] Randomize cache indexing if true (for L2/LLC)
    if (m_is_index_randomized){
      int64_t physical_line_num = bitSelect(address,\
      m_start_index_bit, m_mem_footprint_bits - 1);
      int64_t encrypted_line_num = m_ela_table[physical_line_num];
      cacheset = bitSelect(encrypted_line_num, 0,
                           m_cache_num_set_bits - 1);
      DPRINTF(RubyCache, "Address %lld, PLN %lld, ELN %lld, Set %lld \n",\
              address,physical_line_num,encrypted_line_num,cacheset);
    }
    else {
      cacheset = bitSelect(address, m_start_index_bit,
                     m_start_index_bit + m_cache_num_set_bits - 1);
    }

    return cacheset;
}

// Given a cache index: returns the index of the tag in a set.
// returns -1 if the tag is not found.
int
CacheMemory::findTagInSet(int64_t cacheSet, Addr tag) const
{
    assert(tag == makeLineAddress(tag));
    // search the set for the tags
    auto it = m_tag_index.find(tag);
    if (it != m_tag_index.end())
        if (m_cache[cacheSet][it->second]->m_Permission !=
            AccessPermission_NotPresent)
            return it->second;
    return -1; // Not found
}

// Given a cache index: returns the index of the tag in a set.
// returns -1 if the tag is not found.
int
CacheMemory::findTagInSetIgnorePermissions(int64_t cacheSet,
                                           Addr tag) const
{
    assert(tag == makeLineAddress(tag));
    // search the set for the tags
    auto it = m_tag_index.find(tag);
    if (it != m_tag_index.end())
        return it->second;
    return -1; // Not found
}

// Given an unique cache block identifier (idx): return the valid address
// stored by the cache block.  If the block is invalid/notpresent, the
// function returns the 0 address
Addr
CacheMemory::getAddressAtIdx(int idx) const
{
    Addr tmp(0);

    int set = idx / m_cache_assoc;
    assert(set < m_cache_num_sets);

    int way = idx - set * m_cache_assoc;
    assert (way < m_cache_assoc);

    AbstractCacheEntry* entry = m_cache[set][way];
    if (entry == NULL ||
        entry->m_Permission == AccessPermission_Invalid ||
        entry->m_Permission == AccessPermission_NotPresent) {
        return tmp;
    }
    return entry->m_Address;
}

bool
CacheMemory::tryCacheAccess(Addr address, RubyRequestType type,
                            DataBlock*& data_ptr)
{
    assert(address == makeLineAddress(address));
    DPRINTF(RubyCache, "address: %#x\n", address);
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);
    if (loc != -1) {
        // Do we even have a tag match?
        AbstractCacheEntry* entry = m_cache[cacheSet][loc];
        m_replacementPolicy_ptr->touch(cacheSet, loc, curTick());
        data_ptr = &(entry->getDataBlk());

        if (entry->m_Permission == AccessPermission_Read_Write) {
            return true;
        }
        if ((entry->m_Permission == AccessPermission_Read_Only) &&
            (type == RubyRequestType_LD ||
             type == RubyRequestType_IFETCH ||
             type == RubyRequestType_SPEC_LD)) {
            return true;
        }
        // The line must not be accessible
    }
    data_ptr = NULL;
    return false;
}

bool
CacheMemory::testCacheAccess(Addr address, RubyRequestType type,
                             DataBlock*& data_ptr)
{
    assert(address == makeLineAddress(address));
    DPRINTF(RubyCache, "address: %#x\n", address);
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);

    if (loc != -1) {
        // Do we even have a tag match?
        AbstractCacheEntry* entry = m_cache[cacheSet][loc];
        m_replacementPolicy_ptr->touch(cacheSet, loc, curTick());
        data_ptr = &(entry->getDataBlk());

        return m_cache[cacheSet][loc]->m_Permission !=
            AccessPermission_NotPresent;
    }

    data_ptr = NULL;
    return false;
}

// tests to see if an address is present in the cache
bool
CacheMemory::isTagPresent(Addr address) const
{
    assert(address == makeLineAddress(address));
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);

    if (loc == -1) {
        // We didn't find the tag
        DPRINTF(RubyCache, "No tag match for address: %#x\n", address);
        return false;
    }
    DPRINTF(RubyCache, "TagPresent - address: %#x found\n", address);
    return true;
}

// Returns true if there is:
//   a) a tag match on this address or there is
//   b) an unused line in the same cache "way"
bool
CacheMemory::cacheAvail(Addr address) const
{
    assert(address == makeLineAddress(address));

    int64_t cacheSet = addressToCacheSet(address);

    for (int i = 0; i < m_cache_assoc; i++) {
      bool found_invalid_entry = false;
      
      AbstractCacheEntry* entry = m_cache[cacheSet][i];
      if (entry != NULL) {
        if (entry->m_Address == address) {
          // Already in the cache
          return true;          
        } else if (entry->m_Permission == AccessPermission_NotPresent){
          // We found an empty entry
          found_invalid_entry = true;
        }        
      } else { //Entry is NULL
        found_invalid_entry = true;
      }
      
      
      if(found_invalid_entry){ //Additional checks if sideeffects tracked
        DPRINTF(RubyCache, "**CacheAvail** for Addr: %#x. Found Invalid Entry in Way %d\n",address,i);

        if(!m_tracks_sideeffects)
          return true;

        auto it = mshr.begin();
        bool found_reservation = false;
        while(it != mshr.end()){          
          found_reservation = ((std::get<1>(it->second) == cacheSet) && (std::get<2>(it->second) == i));
          if(found_reservation)
            break;
          ++it;
        }

        if(it != mshr.end())
          DPRINTF(RubyCache, "**CacheAvail** for Addr: %#x. Found_Reservation=%d \n",address,found_reservation);
        
        //if not found reservation or found reservation for current address.
        if( (!found_reservation) || (found_reservation && (makeLineAddress(it->first) == makeLineAddress(address)) ) )
             return true;
      }      
      
    }
    return false;
}

AbstractCacheEntry*
CacheMemory::allocate(Addr address, AbstractCacheEntry *entry, bool touch)
{
    assert(address == makeLineAddress(address));
    assert(!isTagPresent(address));
    assert(cacheAvail(address));
    DPRINTF(RubyCache, "Allocating address: %#x\n", address);

    int64_t cacheSet = addressToCacheSet(address);
    
    //Check if MSHR has reserved spot for this address/
    int reserved_way = -1;
    bool found = false;
    if(m_tracks_sideeffects){
      auto it = mshr.find(address);
      if(it != mshr.end())
        found = (std::get<1>(it->second) == cacheSet);    
      if(found){
        reserved_way = std::get<2>(it->second);
        DPRINTF(RubyCache, "Allocate for addr: %x, found reserved way: %d\n",reserved_way);
      }
    }
    
    // Find the first open slot
    std::vector<AbstractCacheEntry*> &set = m_cache[cacheSet];
    for (int i = 0; i < m_cache_assoc; i++) {
      if (!set[i] || set[i]->m_Permission == AccessPermission_NotPresent) {
        if (set[i] && (set[i] != entry)) {
          warn_once("This protocol contains a cache entry handling bug: "
                    "Entries in the cache should never be NotPresent! If\n"
                    "this entry (%#x) is not tracked elsewhere, it will memory "
                    "leak here. Fix your protocol to eliminate these!",
                    address);
        }

        //If reserved way found for our addr and this way not that one.
        if(m_tracks_sideeffects && found && (reserved_way != i)){
          DPRINTF(RubyCache, "Allocate for addr: %#x, way: %d reserved way %d\n",address,i, reserved_way);
          continue;

        }  else if(m_tracks_sideeffects && (!found)){
          //Check if this way is reserved for someone else
          bool found_way_reserved = false;         
          auto it2 = mshr.begin();
          while(it2 != mshr.end()){
            //MSHR Entry has this set and way.
            found_way_reserved = ((std::get<1>(it2->second) == cacheSet) && (std::get<2>(it2->second) == i) );
            if(found_way_reserved)
              break;              
            ++it2;
          }
          if(found_way_reserved){
            Addr reserved_addr_to_install = (it2->first);
            //Check if way is reserved for some other address.
            DPRINTF(RubyCache, "Allocate for addr: %#x, way: %d reserved for %#x\n",address,std::get<2>(it2->second),std::get<0>(it2->second));
            if(isAddrValid(reserved_addr_to_install)){
              continue;
            }
          }          
        }

        DPRINTF(RubyCache, "Allocating addr: %x in way %d\n",
                address,i);
    
        set[i] = entry;  // Init entry
        set[i]->m_Address = address;
        set[i]->m_Permission = AccessPermission_Invalid;
        DPRINTF(RubyCache, "Allocate clearing lock for addr: %x\n",
                address);
        set[i]->m_locked = -1;
        m_tag_index[address] = i;
        entry->setSetIndex(cacheSet);
        entry->setWayIndex(i);

        if (touch) {
          m_replacementPolicy_ptr->touch(cacheSet, i, curTick());
        }

        return entry;
      }
    }    
    panic("Allocate didn't find an available entry");
}

void
CacheMemory::deallocate(Addr address)
{
    assert(address == makeLineAddress(address));
    assert(isTagPresent(address));
    DPRINTF(RubyCache, "De-allocating address: %#x\n", address);
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);
    if (loc != -1) {
        delete m_cache[cacheSet][loc];
        m_cache[cacheSet][loc] = NULL;
        m_tag_index.erase(address);
    }
}

// Returns with the physical address of the conflicting cache line
Addr
CacheMemory::cacheProbe(Addr address, bool track_sideeffects) 
{
    assert(address == makeLineAddress(address));
    assert(!cacheAvail(address));

    DPRINTF(RubyCache,"CachePROBE for Addr %#x, MSHR Entries:\n",address);
    for(auto it=mshr.begin();it !=mshr.end(); ++it){
      
      DPRINTF(RubyCache,"Addr: %#x -> [%#x,%d,%d]\n",it->first,std::get<0>(it->second),std::get<1>(it->second),std::get<2>(it->second));
    }

    //Check if MSHR has a eviction address reserved for this cacheProbe
    if(mshr.find(address) != mshr.end()){
      auto it = mshr.find(address);
      //If eviction addr is valid.
      Addr EvictionAddr = std::get<0>(it->second);
      if(isAddrValid(EvictionAddr))
        return EvictionAddr;
    }
    
    int64_t cacheSet = addressToCacheSet(address);
    int victim_way = m_replacementPolicy_ptr->getVictim(cacheSet);
    if(m_cache[cacheSet][victim_way] == NULL){ //Line was invalidated for some other install
      assert(m_tracks_sideeffects);
      DPRINTF(RubyCache,"CachePROBE for Addr %#x, found NULL victim in way: %d\n",address,victim_way);
      return ((Addr) -1);
    }
    
    Addr victim_addr =  m_cache[cacheSet][victim_way]-> m_Address;

    if(track_sideeffects && m_tracks_sideeffects){ // Update the side-effect of a miss in MSHR
      assert(m_tracks_sideeffects == true);
      //Check whether victim_addr reserved by another addr:
      auto it = mshr.begin();
      bool found_reservation = false;
      while(it != mshr.end()){
        found_reservation = ((std::get<1>(it->second) == cacheSet) && (std::get<2>(it->second) == victim_way));
        if(found_reservation)
          break;
        ++it;
      }
      DPRINTF(RubyCache,"CachePROBE for Addr %#x, found victim_addr %#x in way: %d, Reserved: %d \n",address,victim_addr,victim_way, found_reservation);
      
      //if evicted line reserved for other address?
      if(found_reservation && (makeLineAddress(it->first) != makeLineAddress(address)) ){
        DPRINTF(RubyCache,"CachePROBE for Addr %#x, found victim_addr %#x in way: %d, Reserved for %#x \n",address,victim_addr,victim_way, makeLineAddress(it->first));
        return ((Addr) -1);
        //assert(0);
      }
      //Proceed if not found reservation or reservation is mine.
      
      //Update the MSHR if not found reservation. 
      if(!found_reservation){
        SideEffectEntry temp_value = std::make_tuple (victim_addr,cacheSet,victim_way);
        auto ret = mshr.insert(std::make_pair(address, temp_value));
        if(ret.second == false){

          DPRINTF(RubyCache,"INSERT FAILED for Addr %#x, MSHR Entries:\n",address);      
          for(auto it=mshr.begin();it !=mshr.end(); ++it){
            DPRINTF(RubyCache,"Addr: %#x -> [%#x,%d,%d]\n",it->first,std::get<0>(it->second),std::get<1>(it->second),std::get<2>(it->second));
          }

          assert(0);
        }
        DPRINTF(RubyCache,"INSERT for Addr %#x, MSHR Entries:\n",address);      
        for(auto it=mshr.begin();it !=mshr.end(); ++it){
          DPRINTF(RubyCache,"Addr: %#x -> [%#x,%d,%d]\n",it->first,std::get<0>(it->second),std::get<1>(it->second),std::get<2>(it->second));
        }
      }
      
    }    
    return victim_addr;
}

// looks an address up in the cache
AbstractCacheEntry*
CacheMemory::lookup(Addr address)
{
    assert(address == makeLineAddress(address));
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);
    if (loc == -1) return NULL;
    return m_cache[cacheSet][loc];
}

// looks an address up in the cache
const AbstractCacheEntry*
CacheMemory::lookup(Addr address) const
{
    assert(address == makeLineAddress(address));
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);
    if (loc == -1) return NULL;
    return m_cache[cacheSet][loc];
}

// Sets the most recently used bit for a cache block
void
CacheMemory::setMRU(Addr address)
{
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);

    if (loc != -1)
        m_replacementPolicy_ptr->touch(cacheSet, loc, curTick());
}

void
CacheMemory::setMRU(const AbstractCacheEntry *e)
{
    uint32_t cacheSet = e->getSetIndex();
    uint32_t loc = e->getWayIndex();
    m_replacementPolicy_ptr->touch(cacheSet, loc, curTick());
}

void
CacheMemory::setMRU(Addr address, int occupancy)
{
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);

    if (loc != -1) {
        if (m_replacementPolicy_ptr->useOccupancy()) {
            (static_cast<WeightedLRUPolicy*>(m_replacementPolicy_ptr))->
                touch(cacheSet, loc, curTick(), occupancy);
        } else {
            m_replacementPolicy_ptr->
                touch(cacheSet, loc, curTick());
        }
    }
}

int
CacheMemory::getReplacementWeight(int64_t set, int64_t loc)
{
    assert(set < m_cache_num_sets);
    assert(loc < m_cache_assoc);
    int ret = 0;
    if (m_cache[set][loc] != NULL) {
        ret = m_cache[set][loc]->getNumValidBlocks();
        assert(ret >= 0);
    }

    return ret;
}

void
CacheMemory::recordCacheContents(int cntrl, CacheRecorder* tr) const
{
    uint64_t warmedUpBlocks = 0;
    uint64_t totalBlocks M5_VAR_USED = (uint64_t)m_cache_num_sets *
                                       (uint64_t)m_cache_assoc;

    for (int i = 0; i < m_cache_num_sets; i++) {
        for (int j = 0; j < m_cache_assoc; j++) {
            if (m_cache[i][j] != NULL) {
                AccessPermission perm = m_cache[i][j]->m_Permission;
                RubyRequestType request_type = RubyRequestType_NULL;
                if (perm == AccessPermission_Read_Only) {
                    if (m_is_instruction_only_cache) {
                        request_type = RubyRequestType_IFETCH;
                    } else {
                        request_type = RubyRequestType_LD;
                    }
                } else if (perm == AccessPermission_Read_Write) {
                    request_type = RubyRequestType_ST;
                }

                if (request_type != RubyRequestType_NULL) {
                    tr->addRecord(cntrl, m_cache[i][j]->m_Address,
                                  0, request_type,
                                  m_replacementPolicy_ptr->getLastAccess(i, j),
                                  m_cache[i][j]->getDataBlk());
                    warmedUpBlocks++;
                }
            }
        }
    }

    DPRINTF(RubyCacheTrace, "%s: %lli blocks of %lli total blocks"
            "recorded %.2f%% \n", name().c_str(), warmedUpBlocks,
            totalBlocks, (float(warmedUpBlocks) / float(totalBlocks)) * 100.0);
}

void
CacheMemory::print(ostream& out) const
{
    out << "Cache dump: " << name() << endl;
    for (int i = 0; i < m_cache_num_sets; i++) {
        for (int j = 0; j < m_cache_assoc; j++) {
            if (m_cache[i][j] != NULL) {
                out << "  Index: " << i
                    << " way: " << j
                    << " entry: " << *m_cache[i][j] << endl;
            } else {
                out << "  Index: " << i
                    << " way: " << j
                    << " entry: NULL" << endl;
            }
        }
    }
}

void
CacheMemory::printData(ostream& out) const
{
    out << "printData() not supported" << endl;
}

void
CacheMemory::setLocked(Addr address, int context)
{
    DPRINTF(RubyCache, "Setting Lock for addr: %#x to %d\n", address, context);
    assert(address == makeLineAddress(address));
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);
    assert(loc != -1);
    m_cache[cacheSet][loc]->setLocked(context);
}

void
CacheMemory::clearLocked(Addr address)
{
    DPRINTF(RubyCache, "Clear Lock for addr: %#x\n", address);
    assert(address == makeLineAddress(address));
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);
    assert(loc != -1);
    m_cache[cacheSet][loc]->clearLocked();
}

bool
CacheMemory::isLocked(Addr address, int context)
{
    assert(address == makeLineAddress(address));
    int64_t cacheSet = addressToCacheSet(address);
    int loc = findTagInSet(cacheSet, address);
    assert(loc != -1);
    DPRINTF(RubyCache, "Testing Lock for addr: %#llx cur %d con %d\n",
            address, m_cache[cacheSet][loc]->m_locked, context);
    return m_cache[cacheSet][loc]->isLocked(context);
}

void
CacheMemory::regStats()
{
    SimObject::regStats();

    m_demand_hits
        .name(name() + ".demand_hits")
        .desc("Number of cache demand hits")
        ;

    m_demand_misses
        .name(name() + ".demand_misses")
        .desc("Number of cache demand misses")
        ;

    m_demand_accesses
        .name(name() + ".demand_accesses")
        .desc("Number of cache demand accesses")
        ;

    m_demand_accesses = m_demand_hits + m_demand_misses;

    m_sw_prefetches
        .name(name() + ".total_sw_prefetches")
        .desc("Number of software prefetches")
        .flags(Stats::nozero)
        ;

    m_hw_prefetches
        .name(name() + ".total_hw_prefetches")
        .desc("Number of hardware prefetches")
        .flags(Stats::nozero)
        ;

    m_prefetches
        .name(name() + ".total_prefetches")
        .desc("Number of prefetches")
        .flags(Stats::nozero)
        ;

    m_prefetches = m_sw_prefetches + m_hw_prefetches;

    m_accessModeType
        .init(RubyRequestType_NUM)
        .name(name() + ".access_mode")
        .flags(Stats::pdf | Stats::total)
        ;
    for (int i = 0; i < RubyAccessMode_NUM; i++) {
        m_accessModeType
            .subname(i, RubyAccessMode_to_string(RubyAccessMode(i)))
            .flags(Stats::nozero)
            ;
    }

    numDataArrayReads
        .name(name() + ".num_data_array_reads")
        .desc("number of data array reads")
        .flags(Stats::nozero)
        ;

    numDataArrayWrites
        .name(name() + ".num_data_array_writes")
        .desc("number of data array writes")
        .flags(Stats::nozero)
        ;

    numTagArrayReads
        .name(name() + ".num_tag_array_reads")
        .desc("number of tag array reads")
        .flags(Stats::nozero)
        ;

    numTagArrayWrites
        .name(name() + ".num_tag_array_writes")
        .desc("number of tag array writes")
        .flags(Stats::nozero)
        ;

    numTagArrayStalls
        .name(name() + ".num_tag_array_stalls")
        .desc("number of stalls caused by tag array")
        .flags(Stats::nozero)
        ;

    numDataArrayStalls
        .name(name() + ".num_data_array_stalls")
        .desc("number of stalls caused by data array")
        .flags(Stats::nozero)
        ;
}

// assumption: SLICC generated files will only call this function
// once **all** resources are granted
void
CacheMemory::recordRequestType(CacheRequestType requestType, Addr addr)
{
    DPRINTF(RubyStats, "Recorded statistic: %s\n",
            CacheRequestType_to_string(requestType));
    switch(requestType) {
    case CacheRequestType_DataArrayRead:
        if (m_resource_stalls)
            dataArray.reserve(addressToCacheSet(addr));
        numDataArrayReads++;
        return;
    case CacheRequestType_DataArrayWrite:
        if (m_resource_stalls)
            dataArray.reserve(addressToCacheSet(addr));
        numDataArrayWrites++;
        return;
    case CacheRequestType_TagArrayRead:
        if (m_resource_stalls)
            tagArray.reserve(addressToCacheSet(addr));
        numTagArrayReads++;
        return;
    case CacheRequestType_TagArrayWrite:
        if (m_resource_stalls)
            tagArray.reserve(addressToCacheSet(addr));
        numTagArrayWrites++;
        return;
    default:
        warn("CacheMemory access_type not found: %s",
             CacheRequestType_to_string(requestType));
    }
}

bool
CacheMemory::checkResourceAvailable(CacheResourceType res, Addr addr)
{
    if (!m_resource_stalls) {
        return true;
    }

    if (res == CacheResourceType_TagArray) {
        if (tagArray.tryAccess(addressToCacheSet(addr))) return true;
        else {
            DPRINTF(RubyResourceStalls,
                    "Tag array stall on addr %#x in set %d\n",
                    addr, addressToCacheSet(addr));
            numTagArrayStalls++;
            return false;
        }
    } else if (res == CacheResourceType_DataArray) {
        if (dataArray.tryAccess(addressToCacheSet(addr))) return true;
        else {
            DPRINTF(RubyResourceStalls,
                    "Data array stall on addr %#x in set %d\n",
                    addr, addressToCacheSet(addr));
            numDataArrayStalls++;
            return false;
        }
    } else {
        assert(false);
        return true;
    }
}

bool
CacheMemory::isBlockInvalid(int64_t cache_set, int64_t loc)
{
  return (m_cache[cache_set][loc]->m_Permission == AccessPermission_Invalid);
}

bool
CacheMemory::isBlockNotBusy(int64_t cache_set, int64_t loc)
{
  return (m_cache[cache_set][loc]->m_Permission != AccessPermission_Busy);
}

// [CleanupCache] For generating random mapping table for randomized caches
void
CacheMemory::gen_rand_table \
(std::vector<int64_t> &m_ela_table, int64_t num_lines_in_mem){
  m_ela_table.resize(0);
  m_ela_table.resize(num_lines_in_mem,-1);

  for (int64_t i =0;i <num_lines_in_mem; i++){
    m_ela_table[i] = i;
  }

  for (int64_t i=0;i <num_lines_in_mem; i++){
    int64_t swap_index = (rand() % num_lines_in_mem);

    //Swap entry[i] and entry[swap_index]
    int64_t temp = m_ela_table[i];
    m_ela_table[i] = m_ela_table[swap_index];
    m_ela_table[swap_index] = temp;
  }
}


// [CleanupCache] Checks if remapping to be performed (every N accesses)
bool
CacheMemory::needs_remap(){
  return ( m_is_index_randomized && (!m_issued_remap) &&\
           (( ((int64_t)(m_demand_hits.value() \
                         + m_demand_misses.value()))  % m_remap_accesses)\
            == (m_remap_accesses - 1)) );

}

// [CleanupCache] Gets destination of remapped addr every N accesses.
Addr
CacheMemory::get_remap_addr(){
  Addr remap_addr = 0;
  //Is it really time for remapping?
  if ( m_is_index_randomized && (!m_issued_remap) &&\
       (( ((int64_t)(m_demand_hits.value() + m_demand_misses.value()))\
          % m_remap_accesses) == (m_remap_accesses - 1)) ){

    //Sufficient to invalidate the Repl_Victim in a Random-Set
    //- Has same effect on performance as CEASER mapping
    int64_t rand_cache_set = ( ((uint64_t)rand()) % m_cache_num_sets );
    int64_t victim_way =  m_replacementPolicy_ptr->getVictim(rand_cache_set);

    DPRINTF(RubyCache, "**Testing Remapping - Rand Cache Set: \
  %lld, Max cache sets: %lld, Victim in Set:%lld \n", rand_cache_set, \
            m_cache_num_sets, \
            m_replacementPolicy_ptr->getVictim(rand_cache_set) ) ;

    int64_t idx = rand_cache_set*m_cache_assoc + victim_way;
    remap_addr = getAddressAtIdx(idx);

    //Did we actually get a non-zero
    //resident address that needs to be invalidated?
    if (remap_addr)
      m_issued_remap = 1;
  }
  return remap_addr;
}

// [CLeanupCache] : Get total demand accesses to cache
int
CacheMemory::get_demand_accesses(){
  return ( ((int64_t)( m_demand_hits.value() \
        + m_demand_misses.value() ) )% m_remap_accesses);
}

// [CleanupCache] Reset
void
CacheMemory::reset_remap_var(){
  m_issued_remap = 0;
  return;
}

// Remove side-effect record from MSHR on completion of miss
Addr
CacheMemory::removeMSHRentry(Addr address){

  assert(address == makeLineAddress(address));
  
  SideEffectTable::iterator it = mshr.find(address);
  if(it == mshr.end())
    return ((Addr)-1);

  Addr evicted_addr = (Addr)-1;
  evicted_addr = std::get<0>(it->second);
  mshr.erase(address);

  DPRINTF(RubyCache,"REMOVE for Addr %#x, MSHR Entries:\n",address);
  for(auto it=mshr.begin();it !=mshr.end(); ++it){
    DPRINTF(RubyCache,"Addr: %#x -> [%#x,%d,%d]\n",it->first,std::get<0>(it->second),std::get<1>(it->second),std::get<2>(it->second));
  }

  return evicted_addr;
}

//Return whether address is valid
bool
CacheMemory::isAddrValid(Addr address){
  if(address == ((Addr) -1) )
    return false;
  else if(address == (makeLineAddress((Addr) -1)) )
    return false;
  else
    return true;
}

//CleanupTable Functions:

int
CacheMemory::cleanup_table_insert(Addr Cleanup_LineAddr, CleanupEntry cleanup_entry){
  assert(Cleanup_LineAddr == makeLineAddress(Cleanup_LineAddr));
  //Set cleanup-entry id
  cleanup_entry.ct_id = next_ct_id;

  //Decide insertion location.
  //Cycles curr_compl_tick =cleanup_entry.load_completion_tick;
  //assert(curr_compl_tick != 0) ; //Now inflight loads are inserted with completion time of 0.
  CleanupTable::iterator insert_loc= ct.end(); //Natural position for inserting into a queue.  
    
  //Need to sort by ascending order of load_completion_times  
  auto it = ct.begin();

  while(it != ct.end()){
    if(it->second.load_completion_tick > cleanup_entry.load_completion_tick)
      break;    
    ++it;
  }
  insert_loc = it;
  
  ct.insert(insert_loc,std::make_pair(Cleanup_LineAddr, cleanup_entry)); //Inserting new entry at insert_loc
  //ct.push_back(std::make_pair(Cleanup_LineAddr, cleanup_entry));

  cleanup_table_count++;
  next_ct_id++;
  DPRINTF(RubyCache,"Inserting in Cleanup Table: Addr: %#x , ct_id: %d\n",Cleanup_LineAddr,cleanup_entry.ct_id);
  return cleanup_entry.ct_id;
}


bool
CacheMemory::cleanup_table_erase(Addr Cleanup_LineAddr, int ct_id){
  assert(Cleanup_LineAddr == makeLineAddress(Cleanup_LineAddr));
  
  CleanupTable::iterator it = find_ct(Cleanup_LineAddr,ct_id);
  
  if(it == ct.end()){
    assert(false);
    return false;
  }  

  ct.erase(it);
  --cleanup_table_count;
  --pending_cleanup_count;
  assert(pending_cleanup_count >=0);
  assert(cleanup_table_count >=0);
  
  DPRINTF(RubyCache,"Erasing from Cleanup Table: Addr: %#x , ct_id: %d\n",Cleanup_LineAddr,ct_id);
  return true; 
}

CleanupEntry
CacheMemory::cleanup_table_find(Addr Cleanup_LineAddr, int ct_id){  
  assert(Cleanup_LineAddr == makeLineAddress(Cleanup_LineAddr));

  DPRINTF(RubyCache,"Finding in Cleanup Table: Addr: %#x , ct_id: %d\n",Cleanup_LineAddr,ct_id);
  CleanupTable::iterator it = find_ct(Cleanup_LineAddr,ct_id);
  if(it == ct.end()){
    assert(false);
  }

  return (it->second);
}

CleanupTable::iterator
CacheMemory::cleanup_table_findit(Addr Cleanup_LineAddr, int ct_id){
  
  assert(Cleanup_LineAddr == makeLineAddress(Cleanup_LineAddr));
  
  CleanupTable::iterator it = find_ct(Cleanup_LineAddr, ct_id);
  if(it == ct.end())
    assert(false);

  return it;
}

bool
CacheMemory::cleanup_needs_evictL1(Addr Cleanup_LineAddr, int ct_id){
  CleanupEntry ce = cleanup_table_find(Cleanup_LineAddr, ct_id);  
  return (ce.toEvict_L1);
}

bool
CacheMemory::cleanup_needs_rollback(Addr Cleanup_LineAddr, int ct_id){
  CleanupEntry ce = cleanup_table_find(Cleanup_LineAddr, ct_id);  
  return (ce.toInstall_L1);
}

Addr
CacheMemory::cleanup_get_rollback_addr(Addr Cleanup_LineAddr, int ct_id){
  CleanupEntry ce = cleanup_table_find(Cleanup_LineAddr, ct_id);
  assert(ce.Rollback_LineAddr == makeLineAddress(ce.Rollback_LineAddr));
  return (ce.Rollback_LineAddr);
}

Addr
CacheMemory::cleanup_get_cleanup_addr(Addr Rollback_LineAddr, int ct_id){
  assert(Rollback_LineAddr == makeLineAddress(Rollback_LineAddr));
  Addr to_ret = ( (Addr) -1); //Default - if not found.
  
  auto it = ct.begin();
  bool found = false;
  while(it != ct.end()){
    found = ((it->second.Rollback_LineAddr == Rollback_LineAddr) && (it->second.ct_id == ct_id));
    if(found)
      break;
    ++it;
  }
      
  if(found)
    to_ret = it->first;    
  
  return (to_ret) ; 
}

bool
CacheMemory::cleanup_needs_evictL2(Addr Cleanup_LineAddr, int ct_id){
  CleanupEntry ce = cleanup_table_find(Cleanup_LineAddr, ct_id);  
  return (ce.toEvict_L2);
}

bool
CacheMemory::cleanup_done(Addr Cleanup_LineAddr, int ct_id){  
  CleanupEntry ce = cleanup_table_find(Cleanup_LineAddr, ct_id);  
  return (ce.doneEvict_L1 && ce.doneInstall_L1 && ce.doneEvict_L2);
}

bool
CacheMemory::cleanup_set_toEvictL1(Addr Cleanup_LineAddr, bool value, int ct_id){
  CleanupTable::iterator it =  cleanup_table_findit(Cleanup_LineAddr, ct_id);
  it->second.toEvict_L1 = value;
  return ( cleanup_table_findit(Cleanup_LineAddr, ct_id)->second.toEvict_L1);
}

bool
CacheMemory::cleanup_set_toEvictL2(Addr Cleanup_LineAddr, bool value, int ct_id){  
  CleanupTable::iterator it =  cleanup_table_findit(Cleanup_LineAddr,ct_id);
  it->second.toEvict_L2 = value;
  return ( cleanup_table_findit(Cleanup_LineAddr,ct_id)->second.toEvict_L2);
}

bool
CacheMemory::cleanup_set_toInstallL1(Addr Cleanup_LineAddr, bool value, int ct_id){
  CleanupTable::iterator it =  cleanup_table_findit(Cleanup_LineAddr,ct_id);
  it->second.toInstall_L1 = value;
  return (cleanup_table_findit(Cleanup_LineAddr,ct_id)->second.toInstall_L1);
}


bool
CacheMemory::cleanup_set_doneEvictL1(Addr Cleanup_LineAddr, bool value, int ct_id){
  CleanupTable::iterator it =  cleanup_table_findit(Cleanup_LineAddr,ct_id);
  it->second.doneEvict_L1 = value;
  return ( cleanup_table_findit(Cleanup_LineAddr,ct_id)->second.doneEvict_L1);
}

bool
CacheMemory::cleanup_set_doneEvictL2(Addr Cleanup_LineAddr, bool value, int ct_id){
  CleanupTable::iterator it =  cleanup_table_findit(Cleanup_LineAddr,ct_id);
  it->second.doneEvict_L2 = value;
  return ( cleanup_table_findit(Cleanup_LineAddr,ct_id)->second.doneEvict_L2);
}

bool
CacheMemory::cleanup_set_doneInstallL1(Addr Cleanup_LineAddr, bool value, int ct_id){
  CleanupTable::iterator it =  cleanup_table_findit(Cleanup_LineAddr,ct_id);
  it->second.doneInstall_L1 = value;
  return ( cleanup_table_findit(Cleanup_LineAddr,ct_id)->second.doneInstall_L1);
}


SideEffectEntry
CacheMemory::getMSHRentry(Addr address){

  assert(address == makeLineAddress(address));

  SideEffectEntry to_ret = std::make_tuple ((Addr)-1,0,0);
  
  SideEffectTable::iterator it = mshr.find(address);
  if(it != mshr.end())
    to_ret = (it->second);
  
  return to_ret;
}

bool
CacheMemory::cleanup_check_addr_involved(Addr LineAddr, int ct_id){
  assert(LineAddr == makeLineAddress(LineAddr));

  //Check if LineAddr matches any RollbackAddr or CleanupAddr
  auto it = ct.begin();
  bool found = false;
  while(it != ct.end()){
    if(it->second.ct_id == ct_id)
      continue;
    
    found = ( (addressToCacheSet(it->second.Rollback_LineAddr) == addressToCacheSet(LineAddr)) \
              || (addressToCacheSet(it->first) == addressToCacheSet(LineAddr)) );
    if(found)
         break;
    ++it;
  }

  return found;  
}

int
CacheMemory::cleanup_table_size(){
  //cleanup_table_dumpcontents("RET-SIZE");
  //  return ct.size();  
  int64_t count = cleanup_table_count;
  return count;
}

void
CacheMemory::cleanup_table_dumpcontents(std::string ctx){
  DPRINTF(RubyCache, "**%s : Cleanup Table Contents: \n",ctx);
  auto it = ct.begin();
  while(it != ct.end()){
    //Addr Cleanup_LineAddr = it->first;
    //CleanupEntry cleanup_entry = it->second;
    DPRINTF(RubyCache, "CleanupLineAddr: %#x -> entry (%#x,%d,%d,%d)\n", \
            it->first, it->second.Rollback_LineAddr,          \
            it->second.toEvict_L1,it->second.toInstall_L1,it->second.toEvict_L2);
    ++it;
  }
}


bool
CacheMemory::cleanup_pending_rollback(Addr LineAddr, int ct_id){
  assert(LineAddr == makeLineAddress(LineAddr));

  //Check if there is any RollbackAddr that matches LineAddr
  auto it = ct.begin();
  bool found = false;
  while(it != ct.end()){        
    found = ( (it->second.Rollback_LineAddr == LineAddr) && \
              (!it->second.doneInstall_L1));
    if(found)
      break;
    ++it;
  }
  return found;  
}


bool
CacheMemory::cleanup_prev_to_set(Addr LineAddr, int ct_id){
  assert(LineAddr == makeLineAddress(LineAddr));

  //Check if pending cleanup to same set after this - checking in reverse.
  
  //auto it = ct.begin();
  auto it = ct.rbegin();
  bool found = false;
  //  while(it != ct.end()){
  while(it != ct.rend()){
    if((it->first == LineAddr) && (it->second.ct_id == ct_id))
      break;
    found = ( ( addressToCacheSet(it->first) == addressToCacheSet(LineAddr)) && \
              ( !(it->second.doneInstall_L1 &&  it->second.doneEvict_L1 &&  it->second.doneEvict_L2 )) );
    if(found){
      DPRINTF(RubyCache,"For Cleanup of Addr: %#x with compTime: %lld, Prev Conflict Found :%d Cleanup Addr: %#x, compTime : %lld\n",LineAddr,find_ct(LineAddr,ct_id)->second.load_completion_tick, found, it->first ,it->second.load_completion_tick);
      break;
    }
    ++it;
  }
  DPRINTF(RubyCache,"For Cleanup of Addr: %#x Prev Conflict Found : %d\n",LineAddr,found);    
  return found;  
}



CleanupTable::iterator CacheMemory::find_ct(Addr LineAddr, int ct_id){
  assert(LineAddr == makeLineAddress(LineAddr));

  auto it = ct.begin();

  while(it != ct.end()){
    if((it->first == LineAddr) && (it->second.ct_id == ct_id)){
      break;
    }
    ++it;
  }
  
  return it;
}

void
CacheMemory::cleanup_pending_count_incr(Addr addr){
  ++pending_cleanup_count;
  DPRINTF(RubyCache,"Request for Addr: %#x. Cleanup Issue Count is Incr to : %d \n",addr, pending_cleanup_count);
}

void
CacheMemory::cleanup_pending_count_decr(Addr addr){
  --pending_cleanup_count;
  DPRINTF(RubyCache,"CleanupAck for Addr: %#x that was a L1-Hit. Cleanup Issue Count ; %d.\n",addr, pending_cleanup_count);
}

bool
CacheMemory::cleanup_issue_pending(){
  DPRINTF(RubyCache,"CleanupIssuePending: %d (Cleanup Issue Count: %d Cleanup Table Count  %d) \n",pending_cleanup_count!= cleanup_table_count,pending_cleanup_count,cleanup_table_count);

  if(pending_cleanup_count == cleanup_table_count){
    //Done - Cleanup will Now Start. Print the Table.
    DPRINTF(RubyCache,"CleanupTable entries:\n");
    auto it = ct.begin();
    while(it!=ct.end()){

      DPRINTF(RubyCache,"**%d** Cleanup%s for [Addr:%#x, ct_id:%d] ->  RollBackAddrL1: %#x At Time %llu InvL1:%d,RollL1:%d,InvL2:%d, Done:%d,%d,%d\n",
              it-ct.begin(),
              it->second.cleanup_type?"Exec":"Inflight",makeLineAddress(it->first),it->second.ct_id,
              it->second.Rollback_LineAddr, it->second.load_completion_tick,
              it->second.toEvict_L1,it->second.toInstall_L1, it->second.toEvict_L2,
              it->second.doneEvict_L1,it->second.doneInstall_L1, it->second.doneEvict_L2);

      ++it;      
    }
  }                        
  return ( pending_cleanup_count != cleanup_table_count );
}

//Check if a data address matches with a cleanup req addr
bool
    CacheMemory::cleanup_datareq_dependancy(Addr LineAddr){
    assert(LineAddr == makeLineAddress(LineAddr));

    auto it = ct.begin();

    while(it != ct.end()){
      if(it->first == LineAddr){
        break;
      }
      ++it;
    }

    return (it != ct.end());
  }


//Check if a data address matches with a cleanup req addr
bool
CacheMemory::cleanup_inflight_received(Addr LineAddr){
  assert(LineAddr == makeLineAddress(LineAddr));

  auto it = ct.begin();

  while(it != ct.end()){
    if( (it->first == LineAddr) && (it->second.cleanup_type == inflight) ){
      break;
    }
    ++it;
  }
  
  return (it != ct.end());
}

//Get constant data block for Ruby 
DataBlock& CacheMemory::getConstDataBlock(){

  //  return constCacheEntry.getDataBlk();
  return constDataBlk;
}
