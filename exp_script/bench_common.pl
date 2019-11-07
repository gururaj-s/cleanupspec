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

$SUITES{'spec2006_final'}      =
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
