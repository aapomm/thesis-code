set ns [new Simulator]
set nf [open tcp.nam w]

$ns namtrace-all $nf

proc finish {} {
	global ns nf
	$ns flush-trace
	close $nf
	exec nam tcp.nam &
	exit 0
}

set n0 [$ns node]
set n1 [$ns node]

$ns duplex-link $n0 $n1 1Mb 10ms DropTail

set tcp0 [new Agent/TCP/Newreno]
set tcp1 [new Agent/TCPSink]

$ns attach-agent $n0 $tcp0
$ns attach-agent $n1 $tcp1

$ns connect $tcp0 $tcp1

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr attach-agent $tcp0

$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
$ns at 5.0 "finish"

$ns run
