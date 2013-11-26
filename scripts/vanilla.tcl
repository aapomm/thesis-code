set ns [new Simulator]
set nf [open out.nam w]

set nd [open RATE.tr w]
$ns trace-all $nd

$ns namtrace-all $nf

proc finish {} {
	global ns nf nd
	$ns flush-trace
	close $nf
	close $nd
	exec nam out.nam &
	exit 0
}

set source [$ns node]
set sctp0 [new Agent/SCTP/Ratehybrid]
$ns attach-agent $source $sctp0

set sink [$ns node]
set sctp1 [new Agent/SCTP/RatehybridSink]
$ns attach-agent $sink $sctp1

$ns connect $sctp0 $sctp1

$ns duplex-link $source $sink 5Mb 20ms DropTail

set cbr0 [new Application/Traffic/CBR]
$cbr0 set packetSize_ 1000
$cbr0 set rate_ 5mb
$cbr0 attach-agent $sctp0

$ns at 1.0 "$cbr0 start"
$ns at 6.0 "finish"

$ns run
