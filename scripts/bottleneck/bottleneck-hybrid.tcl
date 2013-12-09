set ns [new Simulator]
set nf [open bottleneck-hybrid.nam w]

set nd [open bottleneck-hybrid.tr w]
$ns trace-all $nd

$ns namtrace-all $nf

proc finish {} {
	global ns nf nd
	$ns flush-trace
	close $nf
	close $nd
	#exec nam out.nam &
	exit 0
}

set source [$ns node]
set sctp0 [new Agent/SCTP/Ratehybrid]
#$sctp0 set debug_ 1
$sctp0 set fid_ 1
$ns attach-agent $source $sctp0

set sink [$ns node]
set sctp1 [new Agent/SCTP/RatehybridSink]
$ns attach-agent $sink $sctp1

set b1 [$ns node]
set b2 [$ns node]

$ns connect $sctp0 $sctp1

$ns duplex-link $source $b1 100Mb 20ms DropTail
$ns duplex-link $b1 $b2 1Mb 20ms DropTail
$ns duplex-link $b2 $sink 100Mb 20ms DropTail

set cbr0 [new Application/Traffic/CBR]
$cbr0 set packetSize_ 1000
$cbr0 set rate_ 10mb
$cbr0 attach-agent $sctp0

$ns at 1.0 "$cbr0 start"
$ns at 10.0 "finish"

$ns run
