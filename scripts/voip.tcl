# start new simulation
set ns [new Simulator]

# setup tracing/nam
set tr [open voip.tr w]
set nf [open voip.nam w]
$ns trace-all $tr
$ns namtrace-all $nf

# set color of flows
$ns color 2 Red

# finish function, close all trace files and open up nam
proc finish {} {
	global ns nf tr
	$ns flush-trace
	close $nf
	close $tr
	exec nam voip.nam &
	exit 0
}

# creating nodes
# n - sources, r - intermediates, d - destination
set n0 [$ns node]
$n0 label "DCCP"
$n0 color red
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]
set n5 [$ns node]
set r1 [$ns node]
set r2 [$ns node]
set d1 [$ns node]

# creating duplex-links
$ns duplex-link $n0 $r1 10Mb 10ms DropTail
$ns duplex-link $n1 $r1 10Mb 10ms DropTail
$ns duplex-link $n2 $r1 10Mb 10ms DropTail
$ns duplex-link $n3 $r1 10Mb 10ms DropTail
$ns duplex-link $n4 $r1 10Mb 10ms DropTail
$ns duplex-link $n5 $r1 10Mb 10ms DropTail
$ns duplex-link $r1 $r2 0.25Mb 50ms DropTail
$ns duplex-link $r2 $d1 10Mb 10ms DropTail

# Set queue limit
$ns queue-limit $r1 $r2 20

# monitor queue for NAM
$ns duplex-link-op $r1 $r2 queuePos 0.5


# create agents
set dccp0 [new Agent/DCCP/TCPlike]
$ns attach-agent $n0 $dccp0
$dccp0 set fid_ 2
set dccp1 [new Agent/DCCP/TCPlike]
$ns attach-agent $d1 $dccp1
$dccp1 set fid_ 2
set sinknode [new Agent/TCPSink]
$ns attach-agent $d1 $sinknode
set tcp1 [new Agent/TCP]
$ns attach-agent $n1 $tcp1
set tcp2 [new Agent/TCP]
$ns attach-agent $n2 $tcp2
set tcp3 [new Agent/TCP]
$ns attach-agent $n3 $tcp3
set tcp4 [new Agent/TCP]
$ns attach-agent $n4 $tcp4
set tcp5 [new Agent/TCP]
$ns attach-agent $n5 $tcp5

# connect agents
$ns connect $dccp0 $dccp1
$ns connect $tcp1 $sinknode
$ns connect $tcp2 $sinknode
$ns connect $tcp3 $sinknode
$ns connect $tcp4 $sinknode
$ns connect $tcp5 $sinknode

# create CBR applications

set cbr0 [new Application/Traffic/CBR]
$cbr0 set packetSize_ 80
$cbr0 set interval_ 0.030
set cbr1 [new Application/Traffic/CBR]
$cbr1 set packetSize_ 80
$cbr1 set interval_ 0.030
set cbr2 [new Application/Traffic/CBR]
$cbr2 set packetSize_ 80
$cbr2 set interval_ 0.030
set cbr3 [new Application/Traffic/CBR]
$cbr1 set packetSize_ 80
$cbr1 set interval_ 0.030
set cbr4 [new Application/Traffic/CBR]
$cbr1 set packetSize_ 80
$cbr1 set interval_ 0.030
set cbr5 [new Application/Traffic/CBR]
$cbr1 set packetSize_ 80
$cbr1 set interval_ 0.030

# attach agents to applications
$cbr0 attach-agent $dccp0
$cbr1 attach-agent $tcp1
$cbr2 attach-agent $tcp2
$cbr3 attach-agent $tcp3
$cbr4 attach-agent $tcp4
$cbr5 attach-agent $tcp5

# start traffic
$ns at 0.5 "$cbr0 start"
$ns at 0.5 "$cbr1 start"
$ns at 0.5 "$cbr2 start"
$ns at 0.5 "$cbr3 start"
$ns at 0.5 "$cbr4 start"
$ns at 0.5 "$cbr5 start"
$ns at 0.5 "$dccp1 listen"

# stop up traffic
$ns at 10.0 "$cbr0 stop"
$ns at 10.0 "$cbr1 stop"
$ns at 10.0 "$cbr2 stop"
$ns at 10.0 "$cbr3 stop"
$ns at 10.0 "$cbr4 stop"
$ns at 10.0 "$cbr5 stop"

# finish simulation
$ns at 20.0 "finish"

# run the simulation
$ns run
