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
$ns attach-agent $n0 $e

set d [new Agent/Decapsulator]
$ns attach-agent $n1 $d

set udp [new Agent/UDP]
$udp target $e

set udp1 [new Agent/UDP]
$e target 

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr attach-agent $udp

set null [new Agent/Null]
$d target $null

$ns connect $e $d

puts "Connected"

puts "Stopped"

$ns at 0.5 "$cbr start"
$ns at 4.5 "$cbr stop"
$ns run

puts "TAPOS NA."
