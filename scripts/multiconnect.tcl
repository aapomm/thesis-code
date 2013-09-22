Agent/Ping instproc recv {from rtt} {
	$self instvar node_
	puts "node [$node_ id] received ping answer from \
              $from with round-trip-time $rtt ms."
}

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

$ns duplex-link $n0 $n1 1Mb 10ms DropTail
$ns duplex-link $n1 $n2 1Mb 10ms DropTail

set udp0 [new Agent/Ping]
$ns attach-agent $n0 $udp0

set udp1 [new Agent/Ping]
$ns attach-agent $n1 $udp1

set udp2 [new Agent/Ping]
$ns attach-agent $n2 $udp2

$ns connect $udp0 $udp1

$ns at 0.2 "$udp0 send"
$ns at 0.3 "$udp1 send"
$ns at 0.4 "$udp2 send"

$ns at 5.0 "finish"

$ns run
