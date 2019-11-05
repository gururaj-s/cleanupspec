##############################################
# [CleanupCache] Add options specific to cleanup cache
##############################################
from m5 import fatal
import m5.objects

def add_CC_Options(parser):
    # Scheme for simulation
    parser.add_option("--scheme_cleanupcache", default=None,\
                      action="store", type="choice",
                      choices=["UnsafeBaseline",
                               "RandL2", "L1RandRepl", "L1LRURepl", "RandL2_L1RandRepl",
                               "Cleanup_FOR_L1",
                               "Cleanup_FOR_L1L2"],
                      help="choose baseline or defense designs to evaluate")
    # Options for Cleanup_L2
    parser.add_option("--rand_L2", action="store_true", default=False,
                    help="Randomize indexing of L2")

    # Options for Cleanup_L2_L1
    parser.add_option("--cleanupLQExecd_on_Squash", action="store_true", default=False,
                    help="Decides whether to issue Cleanup for Loads Executed in LSQ on Pipeline Squash")

    parser.add_option("--cleanupLQInflight_on_Squash", action="store_true", default=False,
                    help="Decides whether to issue Cleanup for Loads Inflight on Pipeline Squash")

    parser.add_option("--random_repl_L1",  action="store_true", default=False,
                    help="Decides whether to use Random Replacement for L1")

    parser.add_option("--LRU_repl_L1",  action="store_true", default=False,
                    help="Decides whether to use True LRU Replacement for L1")

    parser.add_option("--l1d_mshr_tracking",  action="store_true", default=False,
                    help="Decides whether L1-D Cache tracks side-effects of installs in MSHR")    

    parser.add_option("--L2Inv",  action="store_true", default=False,
                    help="Decides whether to invalidate L2 installs")

    
    parser.add_option("--InternalInterferenceHit",  action="store_true", default=False,
                      help="Decides whether to delay reordered loads, that cause wrong path to accelerate correct path.")

##############################################
# [CleanupCache] : Set scheme-specific options
##############################################

def set_scheme_options(options):
    #default:
    options.rand_L2 = False
    options.cleanupLQExecd_on_Squash = False
    options.cleanupLQInflight_on_Squash = False
    options.random_repl_L1 = False
    options.l1d_mshr_tracking = False
    options.L2inv_on_Squash = False
    options.InternalInterferenceHit = False

    if(options.scheme_cleanupcache == "UnsafeBaseline"):
        pass
    elif(options.scheme_cleanupcache == "RandL2"):
        options.rand_L2 = True
    elif(options.scheme_cleanupcache == "L1RandRepl"):
        options.random_repl_L1 = True
    elif(options.scheme_cleanupcache == "L1LRURepl"):
        options.LRU_repl_L1 = True
    elif(options.scheme_cleanupcache == "RandL2_L1RandRepl"):
        options.random_repl_L1 = True
        options.rand_L2 = True
    elif(options.scheme_cleanupcache == "Cleanup_FOR_L1"):
        options.rand_L2 = True
        options.cleanupLQExecd_on_Squash = True
        options.cleanupLQInflight_on_Squash = True
        options.l1d_mshr_tracking = True
        options.random_repl_L1 = True
    elif(options.scheme_cleanupcache == "Cleanup_FOR_L1L2"):
        options.rand_L2 = True
        options.cleanupLQExecd_on_Squash = True
        options.cleanupLQInflight_on_Squash = True
        options.l1d_mshr_tracking = True
        options.random_repl_L1 = True
        options.L2Inv = True
        options.InternalInterferenceHit = True
    else :
        die("CleanupCache Option: "+options.scheme_cleanupcache+" not supported!\n")
        
    print "**********"
    print "**CleanupCache Simulation Options**"
    print "scheme_cleanupcache=%s"% (options.scheme_cleanupcache)
    print "options.rand_L2=%d"%(options.rand_L2)
    print "options.cleanupLQExecd_on_Squash=%d"%(options.cleanupLQExecd_on_Squash)
    print "options.cleanupLQInflight_on_Squash=%d"%(options.cleanupLQInflight_on_Squash)
    print "options.random_repl_L1=%d"%(options.random_repl_L1)
    print "options.l1d_mshr_tracking=%d"%(options.l1d_mshr_tracking)
    print "options.L2Inv=%d"%(options.L2Inv)
    print "options.InternalInterferenceHit=%d"%(options.InternalInterferenceHit)
    print "**********"
    print " "
    
###############################################   
# [InvisiSpec] Configure the CPU modes
###############################################

def scheme_config_cpu(cpu_cls, cpu_list, options):
    if issubclass(cpu_cls, m5.objects.DerivO3CPU):
        # Assign the same file name to all cpus for now.
        if options.needsTSO==None or options.scheme_invisispec==None:
            fatal("Need to provide needsTSO and scheme "
                "to run simulation with DerivO3CPU")
        
        for cpu in cpu_list:
            #[Invisispec] #####################
            if options.needsTSO:
                cpu.needsTSO = True
            else:
                cpu.needsTSO = False
            if options.allowSpecBuffHit:
                cpu.allowSpecBuffHit = True
            else:
                cpu.allowSpecBuffHit = False
            if len(options.scheme_invisispec)!=0:
                cpu.simulateScheme = options.scheme_invisispec
            #####################################################            
            #[CleanupCache]:
            if options.cleanupLQExecd_on_Squash:
                cpu.cleanupLQExecd_on_Squash = True;
            else:
                cpu.cleanupLQExecd_on_Squash = False;
            if options.cleanupLQInflight_on_Squash:
                cpu.cleanupLQInflight_on_Squash = True;
            else:
                cpu.cleanupLQInflight_on_Squash = False;
            if options.L2Inv:
                cpu.L2inv_on_Squash = True;
            else:
                cpu.L2inv_on_Squash = False;
            if options.InternalInterferenceHit:
                cpu.InternalInterferenceHit = True;
            else:
                cpu.InternalInterferenceHit = False;

                
    else:
        print "not DerivO3CPU"


        
