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

for {set i 0} {$i < 9} {incr i} {
	set n($i) [$ns node]
}

set n9 [$ns node]
set n10 [$ns node]

for {set i 0} {$i < 9} {incr i} {
	$ns duplex-link $n($i) $n9 1Mb 10ms DropTail
}

$ns duplex-link $n9 $n10 1Mb 10ms DropTail

for {set i 0} {$i < 9} {incr i} {
	set sctp($i) [new Agent/SCTP]
	$ns attach-agent $n($i) $sctp($i)
}

set sctp10 [new Agent/SCTP]
$ns attach-agent $n10 $sctp10

for {set i 0} {$i < 9} {incr i} {
	$ns connect $sctp($i) $sctp10
}

for {set i 0} {$i < 9} {incr i} {
	set cbr($i) [new Application/Traffic/CBR]
	$cbr($i) set packetSize_ 500
	$cbr($i) set interval_ 0.05
	$cbr($i) attach-agent $sctp($i)
}

for {set i 0} {$i < 9} {incr i} {
	$ns at 0.2 "$cbr($i) start"
}

for {set i 0} {$i < 9} {incr i} {
	$ns at 1.2 "$cbr($i) stop"
}

$ns at 5.0 "finish"
$ns run
