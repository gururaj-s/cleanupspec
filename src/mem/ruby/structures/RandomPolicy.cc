/*
 * Copyright (c) 2013 Advanced Micro Devices, Inc
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
 *
 * Author: Derek Hower
 */

#include "base/random.hh"
#include "mem/ruby/structures/RandomPolicy.hh"
#include <stdlib.h> 
#include "debug/RubyCache.hh"

RandomPolicy::RandomPolicy(const Params * p)
  : AbstractReplacementPolicy(p)
{
  srand(42);
  //cprintf("RANDOM-REPL");
}


RandomPolicy::~RandomPolicy()
{
}

RandomPolicy *
RandomReplacementPolicyParams::create()
{
    return new RandomPolicy(this);
}


void
RandomPolicy::touch(int64_t set, int64_t index, Tick time)
{
    assert(index >= 0 && index < m_assoc);
    assert(set >= 0 && set < m_num_sets);

    //m_last_ref_ptr[set][index] = time;
}

int64_t
RandomPolicy::getVictim(int64_t set) const
{
  int64_t victim = 0;
  //victim = rand() % m_assoc;
  victim = random_mt.random<unsigned>(0,m_assoc- 1);
  DPRINTF(RubyCache, "GETVICTIM Random fired for set %d, victim way:%d  \n",set,victim);
  assert((victim >= 0) && (victim < m_assoc));  
  return victim;
}
