# CleanupSpec
This repository contains the gem5 implementation of CleanupSpec, a defense mechanism against mis-speculated changes made to the caches.   

Please check our paper for design details:  
**CleanupSpec: An Undo Approach to Safe Speculation.** (https://dl.acm.org/citation.cfm?id=3358314)  
Gururaj Saileshwar and Moinuddin Qureshi  
Proceedings of the 52nd International Symposium on Microarchitecture (**MICRO**), October 2019.  
  
This is built on top of InvisiSpec's fork of Gem5 (Commit:39cfb85 from Nov 5, 2018)  
(https://github.com/mjyan0720/InvisiSpec-1.0/tree/39cfb858d4b2e404282b54094f0220b8098053f6)  

**Requirements:**  
  * Python 2.7 or higher  
  * Other Gem5 Requirements (protobuf, gperftools, etc.)  
  * Tested with Gcc 6.4.0  
    
**Compilation Instructions:**  
  * Set the path to CleanupSpec Gem5 folder in exp_script/env.sh (e.g. GEM5_PATH=<PATH_TO_MICRO19_REPO>)
  * Edit exp_script/env.sh for appropriate paths to other requrements for Gem5.
  * `cd cleanupspec_code ; source exp_script/env.sh ;`
  * `python ``which scons`` build/X86_MESI_Two_Level/gem5.opt -j <NUM_COMPILATION_THREADS>`

**Run Instructions:**  
  * Set the path to SPEC & Checkpoints Directory in exp_script/env.sh (SPEC_PATH, CKPT_PATH)  
  * To create checkpoint: `cd exp_script; ./ckptscript.sh;` (use appropriate $INST_TAKE_CHECKPOINT and $CHECKPOINT_CONFIG)  
  * To run from checkpoint: `cd exp_script; ./runscript.sh;` (use appropriate $SCHEME_CLEANUPSPEC in the script)  
  * To check results: `cd exp_script/stats_scripts ; ./stats_perf.sh`   (Prints CPI normalized to baseline)

**Schemes Supported ($SCHEME_CLEANUPSPEC in exp_script/runscript.sh)**  
  (See configs/common/Scheme.py for more details)  
  * UnsafeBaseline     : Baseline  
  * L1RandRepl         : Random replacement policy for L1-Dcache  
  * RandL2             : Randomized indexing (like CEASER) for L2  
  * RandL2_L1RandRepl  : RandL2 + L1RandRepl  
  * Cleanup_FOR_L1     : L1-Random-Repl + L1-Invalidation + L1-Rollback + L2-Randomized-Indexing  
  * Cleanup_FOR_L1L2   : L1-Random-Repl + L1-Invalidation + L1-Rollback + L2-Randomized-Indexing + L2-Invalidation  

**Major changes to InvisiSpec's fork of gem5.**
  * Modified "DerivO3" classes to provide Cache-Cleanup for squashed loads on a processor mis-prediction.
  * Modified the "MESI_Two_Level" Ruby cache design to support new Cleanup operations.
  * Added side-effect tracking data-structures in MSHRs of  L1-Cache / L2-Cache (in Ruby) and in Load-Queue of processor (in DerivO3).
    
