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

$ns duplex-link $n0 $n1 1Mb 10ms DropTail

set dccp0 [new Agent/DCCP/TCPlike]
set dccp1 [new Agent/DCCP/TCPlike]

$ns attach-agent $n0 $dccp0
$ns attach-agent $n1 $dccp1

$ns connect $dccp0 $dccp1

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 160
$cbr set rate_ 80Kb
$cbr attach-agent $dccp0

$ns at 0.0 "$dccp1 listen"
$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
$ns at 5.0 "finish"

$ns run
