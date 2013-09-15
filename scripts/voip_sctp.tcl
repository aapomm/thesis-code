# start new simulation

set ns [new Simulator]
# setup tracing/nam
set tr [open voip.tr w]
set nf [open voip.nam w]
$ns trace-all $tr
$ns namtrace-all $nf
# finish function, close all trace files and open up nam
proc finish {} {
    global ns nf tr
    $ns flush-trace
    close $nf
    close $tr
    exec nam voip.nam &
    exit 0
}

### creating nodes
set n0 [$ns node]
set n1 [$ns node]

# creating duplex-link
$ns duplex-link $n0 $n1 256Kb 50ms DropTail
$ns duplex-link-op $n0 $n1 orient right

## 2-way VoIP connection
#Create a SCTP agent and attach it to n0
set sctp0 [new Agent/SCTP]
$ns attach-agent $n0 $sctp0
$sctp0 set fid_ 1

set cbr0 [new Application/Traffic/CBR]
$cbr0 set packetSize_ 128
$cbr0 set interval_ 0.020
# set traffic class to 1
$cbr0 set class_ 1
$cbr0 attach-agent $sctp0
# Create a Null sink to receive Data
set sinknode1 [new Agent/LossMonitor]
$ns attach-agent $n1 $sinknode1
set sctp1 [new Agent/SCTP]
$ns attach-agent $n1 $sctp1
$sctp1 set fid_ 2

$ns connect $sctp0 $sctp1
$ns at 0.5 "$cbr0 start"
# stop up traffic
$ns at 4.5 "$cbr0 stop"
# finish simulation
$ns at 5.0 "finish"
# run the simulation
$ns run
