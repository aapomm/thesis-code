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

set sctp0 [new Agent/SCTP]
set sctp1 [new Agent/SCTP]

$ns attach-agent $n0 $sctp0
$ns attach-agent $n1 $sctp1
$ns connect $sctp0 $sctp1

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr attach-agent $sctp0

puts "Done configuring..."
$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
puts "Running simulator..."
$ns run
