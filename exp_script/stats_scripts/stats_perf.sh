cd ..;

pwd
perl getdata.pl  -nh -s system.switch_cpus.cpi_total -w perlbench -n 0 -d  ../output/ooo_4Gmem_100K/Test/UnsafeBaseline ../output/ooo_4Gmem_100K/Test/Cleanup_FOR_L1L2 | column -t 
