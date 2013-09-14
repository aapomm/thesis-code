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

set e [new Agent/Encapsulator]
set d [new Agent/Decapsulator]
set udp0 [new Agent/UDP]
set udp1 [new Agent/UDP]
set udp2 [new Agent/UDP]

$ns attach-agent $n0 $udp1
$ns attach-agent $n1 $udp2
$udp0 target $e
$e decap-target $d
$udp2 target $d

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr attach-agent $udp0

$ns connect $udp1 $udp2

$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
$ns run
