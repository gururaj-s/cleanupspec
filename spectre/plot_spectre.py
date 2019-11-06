#include
import re
import collections
import matplotlib.pyplot as plt
import numpy
import sys, getopt
import seaborn as sns
#sns.set()

#defines: 
array2_size_addr = "0xa70a0" #Address of the array2_size variable (obtained by manually reading runscript.log)
array2_start_addr = "0xab020" #Address of the array2 start (obtained by manually reading runscript.log)

num_phases_plot = 100 #Number of iterations of the attack to plot.
array2_sz_plot  = 64 #Number of array-entries to plot (X-axis). We restrict it to 64 since we know secret is 50.

#code
#functions
def get_plot_xy (file_str):
    #Identify the flushes.
    flush_list = []
    with open(file_str) as f:
        for line in f:
            #Figure out if Flush / Read
            flush_result = flush_pattern.search(line);
            read_result  = read_pattern.search(line);
            #print flush_result
            if(flush_result) :
                flush_str = flush_result.group(0)
                addr_result  = addr_pattern.search(flush_str)
                tick_result = tick_pattern.search(line);
                flush_list.append((tick_result.group(0),addr_result.group(0)))
                #print "Flush:" + str(addr_result.group(0))
    """
    for (tick,addr) in flush_list:
        print tick+", Flush "+addr
    """
        
    ######Identify the Reload Tick-Ranges in the Flush+Reload:
    # Identify last flush to array2_size
    # Identify first flush to array2_start
    # Reload phase from array2_size flush to array2_start flush
    # - Add each phase to reload_phase_list
    ######
    
    last_tick=0
    last_addr=0
    reload_phase_list = []
    
    for (tick,addr) in flush_list:
        if(last_tick):
            if((last_addr != addr) and (last_addr == array2_size_addr)):
                assert(addr == array2_start_addr)
                reload_phase_list.append((last_tick,tick))
        last_tick = tick
        last_addr = addr
    """    
    for (start_addr,end_addr) in reload_phase_list:
        print "Reload From: "+ start_addr + ","  + end_addr
    """
    ######Capture all the latency of Reload accesses to array2 addrs:
    #For each Reload Phase:
    # Identify the read accesses that fall to array2 addrs
    # Add those to appropriate phase_list as a tuple of tick,addr,latency
    ######
    
    phase_active = False
    curr_phase = 0
    phase_accesses_list = [] #PhaseNum X List of (Tick,Addr,Latency)
    phase_accesses_list.append([])
    
    with open(file_str) as f:
        for line in f:
            if(read_pattern.search(line) == None):
                continue
            
            curr_tick = tick_pattern.search(line).group(0)        
            while( (curr_tick > reload_phase_list[curr_phase][1]) ):
                phase_active = False
                curr_phase +=1
                if(curr_phase >= len(reload_phase_list)):
                    break
                phase_accesses_list.append([])            
            if(curr_phase >= len(reload_phase_list)):
                break
    
            #Curr Tick is at least <= curr_phase EndTick
            if( (curr_tick >= reload_phase_list[curr_phase][0]) and  (curr_tick <= reload_phase_list[curr_phase][1]) ):
                phase_active = True
                read_result  = read_pattern.search(line);
                addr  = addr_pattern.search(read_result.group(0)).group(0)
                latency = latency_pattern.search(line).group(0)
                if(array2_addr_pattern.search(addr)):
                    read_access = (curr_tick,addr,latency)
                    phase_accesses_list[curr_phase].append(read_access)
    """                
    for idx,phase_accesses in enumerate(phase_accesses_list):
        print "Phase " +str(idx)
        for tick,addr,latency in phase_accesses:
            print tick + ": Addr "+addr +", "+latency+" cycles"
    """
    ##############Sum and Average Per Array2 Addr
    # Sort addresses:
    # -E,g, i=0 array2[13], i=1 array2[180] .. i=255 array2[102] => array2[0 .. 255]
    #-Add running sum for each entry, and calculate average across all reload tick-ranges.
    
    
    count_phases =0
    latency_dict = {}
    
    plot_x = []
    plot_y = []
    idx_start = len(phase_accesses_list) - num_phases_plot
    assert(idx_start >= 0)

    print "Number of phases for "+ file_str + ": "+str(idx_start)+ "=> "+str(len(phase_accesses_list))
    print "Number of reload addr per phase: "+ str(len(phase_accesses_list[0]))

    for idx,phase_accesses in enumerate(phase_accesses_list):
        if(idx >= idx_start):
            for tick,addr,latency in phase_accesses:
                if(addr in latency_dict):
                    latency_dict[addr] += int(latency)
                else :
                    latency_dict[addr] = int(latency)
                print "Addr:"+addr+" Latency:"+latency+"\n"
            count_phases += 1
            
    
    for addr in sorted(latency_dict):
        print "%s : %.2f" %(addr, latency_dict[addr]/count_phases)
        plot_x.append( numpy.floor((int(addr,0) - int(array2_start_addr,0))/512))
        plot_y.append(latency_dict[addr]/count_phases)

    return plot_x,plot_y

##################### End function ###################################

#regexes
flush_pattern = re.compile(r"Flush\: Addr\=\[0x[a-z0-9]+")
read_pattern = re.compile(r"Read\: Addr\=\[0x[a-z0-9]+")
addr_pattern = re.compile(r"0x[a-z0-9]+")
tick_pattern = re.compile(r"^[0-9]+")
latency_pattern = re.compile(r"[0-9]+$")
array2_addr_pattern = re.compile(r"0x(ab|ac|ad|ae|af|(c[0-9a])|(b[a-f0-9]))[02468ace]20")


#command-line inputs
file_str =[]
if(len(sys.argv) is 3):
    file_str_list = sys.argv[:]
    file_str_list.pop(0)
else:
    print("Expected Command Line: python plot_spectre.py <PATH_TO_BASELINE_RUNSCRIPT.log> <PATH_TO_CLEANUPSPEC_RUNSCRIPT.log>")
    exit(1)
    
plotx_list = []
ploty_list = []
for file_str in file_str_list:
    plotx,ploty = get_plot_xy(file_str)
    plotx_list.append(plotx)
    ploty_list.append(ploty)
            
#Plot LinePlot.
plt.rcParams["font.family"] = "sans-serif"
plt.rcParams["font.size"] = 16


#sns.set_style("darkgrid")
fig1 = plt.figure(figsize=(8,5))
ax = plt.axes()

ax.plot(plotx_list[0],ploty_list[0], color='red',linestyle='-',label='NonSecure')
ax.plot(plotx_list[1],ploty_list[1], color='blue',linestyle='--',label='CleanupSpec')
ax.legend(loc='upper right')

ax.set_axisbelow(True)
ax.yaxis.grid(color='gray', linestyle='dashed')
ax.xaxis.grid(color='gray', linestyle='dashed')

plt.ylim(top=250)
plt.ylim(bottom=75)
plt.xlim(right=array2_sz_plot)
plt.xlim(0)

plt.xlabel("Array Index")
plt.ylabel("Avg. Access Latency (cycles)")

#plt.figure(figsize=(3,1),dpi=1000)
plt.savefig('spectre_latency.eps',format='eps',dpi=1000)
