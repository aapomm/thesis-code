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

set udp0 [new Agent/UDP]
set udp1 [new Agent/UDP]

$ns attach-agent $n0 $udp0
$ns attach-agent $n1 $udp1
$ns connect $udp0 $udp1

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr attach-agent $udp0

puts "Done configuring..."
$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
puts "Running simulator..."
$ns at 5.0 "finish"
$ns run
