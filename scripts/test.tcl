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

set s0 [$ns node]
set s1 [$ns node]
set s2 [$ns node]

set d0 [$ns node]
set d1 [$ns node]
set d2 [$ns node]

set r0 [$ns node]

$ns duplex-link $s0 $s1 1Mb 10ms DropTail
$ns duplex-link $s1 $s2 1Mb 10ms DropTail
$ns duplex-link $d2 $d1 1Mb 10ms DropTail
$ns duplex-link $d1 $d0 1Mb 10ms DropTail

$ns duplex-link $s2 $r0 0.25Mb 100ms DropTail
$ns duplex-link $d2 $r0 0.25Mb 100ms DropTail

set Ssctp [new Agent/SCTP]
$ns attach-agent $s0 $Ssctp

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.005
$cbr attach-agent $Ssctp

set Sdtls [new Agent/DTLS]
$ns attach-agent $s1 $Sdtls

set Sudp [new Agent/UDP]
$ns attach-agent $s2 $Sudp

set Dudp [new Agent/UDP]
$ns attach-agent $d2 $Dudp

set Ddtls [new Agent/DTLS]
$ns attach-agent $d1 $Ddtls

set Dsctp [new Agent/SCTP]
$ns attach-agent $d0 $Dsctp

$ns connect $Ssctp $Sdtls
$ns connect $Ssctp $Dsctp

$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
$ns at 5.0 "finish"

$ns run
