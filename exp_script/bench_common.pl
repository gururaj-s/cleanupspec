#******************************************************************************
# Benchmark Sets
# ************************************************************

%SUITES = ();

#***************************************************************
# Exception
#**************************************************************
$SUITES{'h264ref'}	=
    'h264ref';

$SUITES{'sjeng'}	=
    'sjeng';

$SUITES{'wrf'}	=
    'wrf';

$SUITES{'sphinx3'}	=
    'sphinx3';

$SUITES{'perlbench'}	=
    'perlbench';

$SUITES{'gcc'}	=
    'gcc';

$SUITES{'soplex'}	=
    'soplex';

$SUITES{'bzip2'}	=
    'bzip2';

$SUITES{'gromacs'}	=
    'gromacs';

$SUITES{'astar'}	=
    'astar';

$SUITES{'mcf'}	=
    'mcf';

$SUITES{'milc'}	=
    'milc';

$SUITES{'lbm'}	=
    'lbm';

$SUITES{'hmmer'}	=
    'hmmer';

$SUITES{'gobmk'}	=
    'gobmk';

$SUITES{'povray'}	=
    'povray';

$SUITES{'namd'}	=
    'namd';

#***************************************************************
# SPEC2006 SUITE 
#***************************************************************


$SUITES{'temp'}      =
'astar
bzip2
gcc
gromacs
gobmk
hmmer
h264ref
mcf
omnetpp
soplex
lbm';


$SUITES{'spec2006'}      = 
'mcf 
lbm
soplex
milc
libquantum
omnetpp
bwaves
gcc
sphinx
GemsFDTD
leslie3d
wrf
cactusADM
zeusmp
bzip2
dealII
xalancbmk
hmmer
perlbench
h264ref
astar
gromacs
gobmk
sjeng
namd
tonto
calculix
gamess
povray';

$SUITES{'spec2006_invisispec'}      =
'perlbench
bzip2
mcf 
gobmk
hmmer
sjeng
libquantum
h264ref
omnetpp
astar
bwaves
milc
zeusmp
gromacs
cactusADM
namd
soplex
calculix
GemsFDTD
tonto
lbm
sphinx
gcc
wrf
dealII
povray
xalancbmk
gamess
leslie3d';


$SUITES{'spec2006_checkpoint_1bn'}      =
'astar
bwaves
bzip2
cactusADM
gcc
gobmk
gromacs
h264ref
hmmer
lbm
libquantum
mcf
milc
namd
omnetpp
perlbench
povray
sjeng
soplex
sphinx3
wrf
zeusmp';



$SUITES{'spec2006_checkpoint_10bn'}      =
'mcf
lbm
astar
bzip2
cactusADM
gcc
gobmk
gromacs
h264ref
hmmer
libquantum
milc
namd
omnetpp
perlbench
povray
sjeng
soplex
sphinx3
wrf';

$SUITES{'spec2006_17'}      =
'perlbench
bzip2
mcf 
gobmk
hmmer
sjeng
libquantum
h264ref
omnetpp
astar
bwaves
milc
zeusmp
gromacs
cactusADM
namd
soplex';


$SUITES{'spec2006_checkpoint_10bn_HUNG'}      =
'bzip2
cactusADM
gromacs
h264ref
hmmer
lbm
mcf
omnetpp
perlbench
soplex
sphinx3';

$SUITES{'parsec'}      =
'blackscholes                                                                              
canneal                                                                                    
ferret                                                                                 
fluidanimate                                                                           
freqmine                                                                               
swaptions                                                                              
vips                                                                                   
x264';

$SUITES{'spec2006_final'}      =
'astar
gobmk
sjeng
bzip2
perlbench
povray
gromacs
h264ref
namd
sphinx3
wrf
hmmer
mcf
soplex
gcc
lbm
cactusADM
milc
libquantum';

$SUITES{'spec2006_temp'}      =
'gobmk
sjeng
bzip2
perlbench
povray
gromacs
h264ref
namd
sphinx3
wrf
hmmer
mcf
soplex
gcc
lbm
cactusADM
milc
libquantum';

$SUITES{'spec2006_final_SIMPLEMEM'}      =
'astar
gobmk
sjeng
bzip2
perlbench
povray
gromacs
h264ref
namd
wrf
hmmer
mcf
soplex
gcc
lbm
cactusADM
milc
libquantum';
