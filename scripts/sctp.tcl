set ns [new Simulator]
set nf [open out.nam w]

$ns namtrace-all $nf

proc finish {} {
	global ns nf
	$ns flush-trace
	close $nf
	exec nam out.nam &
	exit 0
}

set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]

$ns duplex-link $n0 $n2 1Mb 10ms DropTail
$ns duplex-link $n1 $n2 1Mb 10ms DropTail
$ns duplex-link $n2 $n3 1Mb 10ms DropTail

set sctp0 [new Agent/SCTP]
set sctp1 [new Agent/SCTP]
set sctp2 [new Agent/SCTP]

$ns attach-agent $n0 $sctp0
$ns attach-agent $n1 $sctp1
$ns attach-agent $n3 $sctp2

$ns connect $sctp0 $sctp2
$ns connect $sctp1 $sctp2

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr attach-agent $sctp0

set cbr1 [new Application/Traffic/CBR]
$cbr1 set packetSize_ 500
$cbr1 set interval_ 0.05
$cbr1 attach-agent $sctp1

$ns at 0.5 "$cbr start"
$ns at 0.5 "$cbr1 start"
$ns at 4.5 "$cbr stop"
$ns at 4.5 "$cbr1 stop"
$ns at 5.0 "finish"
$ns run
