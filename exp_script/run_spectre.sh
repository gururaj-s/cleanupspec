#!/bin/bash

############### INSTRUCTIONS #########

# Run spectre for Baseline & CleanupSpec cofnigurations with the following commands
# Command: ./run_spectre.sh <CONFIG>
# For Baseline: ./run_spectre.sh UnsafeBaseline >../spectre/base.log  2>&1 &
# For CleanupSpec: ./run_spectre.sh Cleanup_FOR_L1L2 >../spectre/cs.log  2>&1 &
# To get the results: cd spectre; python plot_spectre.py results/Test_Spectre/UnsafeBaseline/runscript.log results/Test_Spectre/Cleanup_FOR_L1L2/runscript.log 

########################################

RUN_CONFIG="Test_Spectre"
SPECTRE_DIR="../spectre"
BENCHMARK="spectre"

############## GET SCHEME AND FOLDER ####################################################
 
ARGC=$# # Get number of arguments excluding arg0.
if [[ "$ARGC" != 1 ]]; then # Bad number of arguments.
    echo "EXAMPLE: ./run_spectre.sh [UnsafeBaseline OR Cleanup_FOR_L1L2]"
    echo ""
    exit
fi
 
# Get command line input. We will need to check these.
SCHEME_CLEANUPCACHE=$1

############ GET PATH VARIABLE #############
if [ -z ${GEM5_PATH+x} ];
then
    echo "GEM5_PATH is unset";
    exit
else
    echo "GEM5_PATH is set to '$GEM5_PATH'";
fi
 
##################################################################
# Outdir & Rundir

OUTPUT_DIR=$SPECTRE_DIR/results/$RUN_CONFIG/$SCHEME_CLEANUPCACHE

echo "output directory: " $OUTPUT_DIR

if [ -d "$OUTPUT_DIR" ]
then
    rm -r $OUTPUT_DIR
fi
mkdir -p $OUTPUT_DIR

RUN_DIR=$SPECTRE_DIR
SCRIPT_OUT=$OUTPUT_DIR/runscript.log  # File log for this script's stdout henceforth

################## REPORT SCRIPT CONFIGURATION ###################

echo "Command line:"                                | tee $SCRIPT_OUT
echo "$0 $*"                                        | tee -a $SCRIPT_OUT
echo "================= Hardcoded directories ==================" | tee -a $SCRIPT_OUT
echo "GEM5_PATH:                                     $GEM5_PATH" | tee -a $SCRIPT_OUT
echo "SPEC_PATH:                                     $SPEC_PATH" | tee -a $SCRIPT_OUT
echo "==================== Script inputs =======================" | tee -a $SCRIPT_OUT
echo "BENCHMARK:                                    $BENCHMARK" | tee -a $SCRIPT_OUT
echo "OUTPUT_DIR:                                   $OUTPUT_DIR" | tee -a $SCRIPT_OUT
echo "==========================================================" | tee -a $SCRIPT_OUT
##################################################################


#################### LAUNCH GEM5 SIMULATION ######################
echo ""
echo "Changing to benchmark runtime directory: $RUN_DIR" | tee -a $SCRIPT_OUT
cd $RUN_DIR

echo "" | tee -a $SCRIPT_OUT
echo "" | tee -a $SCRIPT_OUT
echo "--------- Here goes nothing! Starting gem5! ------------" | tee -a $SCRIPT_OUT
echo "" | tee -a $SCRIPT_OUT
echo "" | tee -a $SCRIPT_OUT

# Actually launch gem5!
# --debug-flags=RubySlicc,RubyPort,Fetch,Decode,Rename,IEW,Activity \
$GEM5_PATH/build/X86_MESI_Two_Level/gem5.opt \
              --debug-flags=RubyReadLatency --debug-start=12000000000           \
              --outdir=$OUTPUT_DIR $GEM5_PATH/configs/example/spectre_config.py    \
              --benchmark=$BENCHMARK --benchmark_stdout=$OUTPUT_DIR/$BENCHMARK.out \
              --benchmark_stderr=$OUTPUT_DIR/$BENCHMARK.err \
              --num-cpus=1 --mem-size=4GB \
              --l1d_assoc=8 --l2_assoc=16 --l1i_assoc=4 \
              --cpu-type=DerivO3CPU  --scheme_invisispec=UnsafeBaseline --needsTSO=0 \
              --scheme_cleanupcache=$SCHEME_CLEANUPCACHE \
              --num-dirs=1 --ruby --maxinsts=100000000 --prog-interval=0.003MHz     \
              --network=simple --topology=Mesh_XY --mesh-rows=1 | tee -a $SCRIPT_OUT
