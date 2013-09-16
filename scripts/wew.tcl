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
set sctp0 [new Agent/SCTP]
set sctp1 [new Agent/SCTP]

$ns attach-agent $n0 $udp0
$ns attach-agent $n1 $udp1
#$sctp0 set fid_ 1
#$sctp0 target $e
#$e set status_ 1
#$e target $udp0
#$udp1 target $d
#$d target $sctp1

$ns fix-bindings $sctp0
$ns fix-bindings $sctp1
#$ns connect $udp0 $udp1
$ns multihome-connect $sctp0 $sctp1
$ns simplex-connect $sctp0 $sctp1
$ns simplex-connect $sctp1 $sctp0

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 128
$cbr set interval_ 0.020
$cbr set class_ 1
$cbr attach-agent $sctp0

$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
$ns at 5.0 "finish"
$ns run
