CSCI5273-link-state-routing
===========================

CSCI5273-link-state-routing
By Xinying Zeng
Nov. 15 2013


To start router:
use commands:
./routed_LS A log_A.log PA3_initialization.txt
./routed_LS B log_B.log PA3_initialization.txt
./routed_LS C log_C.log PA3_initialization.txt
./routed_LS D log_D.log PA3_initialization.txt
./routed_LS E log_E.log PA3_initialization.txt
./routed_LS F log_F.log PA3_initialization.txt

To shut down the system, use CTRL-C to kill the process, the log file is saved.

All sockets are set to NON-BLOCK mode using fcntl call. 

After start up, router learn its direct neighbors from configuration file.
