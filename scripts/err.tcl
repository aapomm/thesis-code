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

set dccp0 [new Agent/SCTP]
$ns attach-agent $n0 $dccp0

set dccp1 [new Agent/SCTP]
$ns attach-agent $n1 $dccp1

$ns connect $dccp0 $dccp1

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr attach-agent $dccp0

#SETUP LOSS MODEL
set em [new ErrorModel]
$em unit pkt
$em set rate_ 0.02
$em ranvar [new RandomVariable/Uniform]
$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n0 $n1

$ns at 0.5 "$cbr start"
$ns at 0.5 "$dccp1 listen"
$ns at 5.0 "finish"

$ns run
