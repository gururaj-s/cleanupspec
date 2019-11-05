#Source paths for GCC version 6.4.0
source /home/gattaca4/gururaj/LOCAL_LIB/env_gcc_binutils.sh

#Source path for protobuf and gperftools recommended for Gem5
# (http://learning.gem5.org/book/part1/building.html)
source /home/gattaca4/gururaj/LOCAL_LIB/protobuf/env_protobuf.sh
source /home/gattaca4/gururaj/LOCAL_LIB/gperftools/env_gperftools.sh

#Source path for python 2.7 or higher
source /home/gattaca4/gururaj/LOCAL_LIB/python/env_anaconda2.sh

#Paths for Gem5 & SPEC
export GEM5_PATH=/home/gattaca4/gururaj/CCache/MICRO19_REPO/cleanupspec_code
export SPEC_PATH=/home/gattaca5/gururaj/SPEC2006
export CKPT_PATH=/home/gattaca4/gururaj/CCache/gem5_ckpt
